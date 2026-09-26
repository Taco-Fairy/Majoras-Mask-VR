void InitAndroidInstance() {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity)
        throw std::runtime_error("SDL Android activity unavailable");
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    PFN_xrInitializeLoaderKHR initialize = nullptr;
    Check(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                reinterpret_cast<PFN_xrVoidFunction*>(&initialize)),
          "Find Android loader initialization");
    XrLoaderInitInfoAndroidKHR loader{ XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
    loader.applicationVM = vm;
    loader.applicationContext = activity;
    Check(initialize(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loader)),
          "Initialize Android OpenXR loader");
    const auto extensions =
        Extensions({ XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME });
    XrInstanceCreateInfoAndroidKHR android{ XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
    android.applicationVM = vm;
    android.applicationActivity = activity;
    XrInstanceCreateInfo info{ XR_TYPE_INSTANCE_CREATE_INFO };
    info.next = &android;
    CopyName(info.applicationInfo.applicationName, "Majoras Mask VR");
    CopyName(info.applicationInfo.engineName, "FullDiveGames MMVR");
    info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    info.enabledExtensionCount = extensions.size();
    info.enabledExtensionNames = extensions.data();
    const auto result = xrCreateInstance(&info, &instance);
    env->DeleteLocalRef(activity);
    Check(result, "Create Android OpenXR instance");
}
XrGraphicsBindingOpenGLESAndroidKHR AndroidGraphicsBinding() {
    PFN_xrGetOpenGLESGraphicsRequirementsKHR requirementsFn = nullptr;
    Check(xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                                reinterpret_cast<PFN_xrVoidFunction*>(&requirementsFn)),
          "Find GLES requirements");
    XrGraphicsRequirementsOpenGLESKHR requirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR };
    Check(requirementsFn(instance, system, &requirements), "Read GLES requirements");
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    auto version = XR_MAKE_VERSION(major, minor, 0);
    if (version < requirements.minApiVersionSupported || version > requirements.maxApiVersionSupported)
        throw std::runtime_error("GLES context outside OpenXR supported range");
    XrGraphicsBindingOpenGLESAndroidKHR binding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR };
    binding.display = eglGetCurrentDisplay();
    binding.context = eglGetCurrentContext();
    if (binding.display == EGL_NO_DISPLAY || binding.context == EGL_NO_CONTEXT)
        throw std::runtime_error("No current GLES context");
    EGLint configId = 0;
    eglQueryContext(binding.display, binding.context, EGL_CONFIG_ID, &configId);
    const EGLint attributes[] = { EGL_CONFIG_ID, configId, EGL_NONE };
    EGLint count = 0;
    if (!eglChooseConfig(binding.display, attributes, &binding.config, 1, &count) || count != 1)
        throw std::runtime_error("Cannot resolve SDL EGL configuration");
    return binding;
}
