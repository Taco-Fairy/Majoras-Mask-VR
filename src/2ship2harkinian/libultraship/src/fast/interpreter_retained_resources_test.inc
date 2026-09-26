// Included inside namespace Fast. Exercises cross-tick resource retention with
// the live resource manager and an isolated interpreter, restoring every
// manager change it makes. Writes native-retained-resources.json.
void Interpreter::VerifyRetainedResources(const char* signaturePath, uint64_t hash) {
    const char* test = std::getenv("MMVR_NATIVE_TEST");
    if (!test || std::strcmp(test, "1") != 0 || !signaturePath) return;
    if (!RetainedResourcesEnabled()) {
        std::ofstream("native-retained-resources.json") << "{\"reference\":true}";
        return;
    }
    const auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    std::string_view base = signaturePath;
    if (base.starts_with("__OTR__")) base.remove_prefix(7);
    const std::string basePath(base);
    Interpreter probe;
    unsigned checks = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) throw std::runtime_error(std::string("Retained resource regression: ") + what);
    };
    auto tick = [&](auto&& body) {
        probe.SetResolvedResourceCacheEnabled(true);
        body();
        probe.SetResolvedResourceCacheEnabled(false);
    };

    // 1. A later tick reuses the resolution by content, from a different pointer.
    std::shared_ptr<Ship::IResource> first;
    void* firstRaw = nullptr;
    tick([&] {
        first = probe.ResolveResourceCached(signaturePath);
        firstRaw = probe.ResolveResourceRawCached(hash);
    });
    check(first && firstRaw == first->GetRawPointer(), "initial resolution");
    const std::string copy(signaturePath);
    auto hits = probe.mRetainedResourceDiagnostics.hits;
    tick([&] {
        check(probe.ResolveResourceCached(copy.c_str()) == first, "path reuse");
        check(probe.ResolveResourceRawCached(hash) == firstRaw, "hash reuse");
    });
    check(probe.mRetainedResourceDiagnostics.hits >= hits + 2, "reuse was not retained");

    // 2. A reused pointer holding different text must not return the old resource.
    std::array<char, 256> buffer{};
    check(copy.size() < buffer.size(), "fixture path length");
    std::memcpy(buffer.data(), copy.c_str(), copy.size() + 1);
    tick([&] { check(probe.ResolveResourceCached(buffer.data()) == first, "buffer resolution"); });
    constexpr std::string_view missing = "__mmvr_missing_retained_resource_probe__";
    std::memcpy(buffer.data(), missing.data(), missing.size());
    buffer[missing.size()] = 0;
    tick([&] { check(probe.ResolveResourceCached(buffer.data()) == nullptr, "pointer reuse with new text"); });

    // 3. Replacements are observed between ticks and within a tick. With
    // alternate assets on, an alt override would win over the base key, so the
    // base-key replacement is only meaningful (and safely restorable) when off.
    const bool alt = manager->IsAltAssetsEnabled();
    if (!alt) {
        // A separately owned load of the same file is a distinct, valid object.
        auto replacement = manager->LoadResource(Ship::ResourceIdentifier{ basePath, 0x4d565252, nullptr });
        check(replacement && replacement != first, "distinct replacement");
        const auto generation = manager->GetResolutionGeneration();
        manager->CacheExternalResource(basePath, replacement);
        check(manager->GetResolutionGeneration() != generation, "replacement advanced generation");
        tick([&] {
            check(probe.ResolveResourceCached(copy.c_str()) == replacement, "replacement path");
            check(probe.ResolveResourceRawCached(hash) == replacement->GetRawPointer(), "replacement hash");
        });
        // Mid-tick: the path is already owned by this tick, the hash is not yet.
        tick([&] {
            check(probe.ResolveResourceCached(copy.c_str()) == replacement, "retained replacement");
            manager->CacheExternalResource(basePath, first);
            check(probe.ResolveResourceRawCached(hash) == first->GetRawPointer(), "mid-tick restore observed");
        });
        tick([&] { check(probe.ResolveResourceCached(copy.c_str()) == first, "original restored"); });
    }

    // 4. Alternate-asset mode changes discard every retained answer.
    auto clears = probe.mRetainedResourceDiagnostics.clears;
    manager->SetAltAssetsEnabled(!alt);
    tick([&] {});
    manager->SetAltAssetsEnabled(alt);
    check(probe.mRetainedResourceDiagnostics.clears > clears, "alt toggle clear");

    // 5. Texture-cache invalidation does not change how resources resolve.
    tick([&] { check(probe.ResolveResourceCached(copy.c_str()) == first, "before texture clear"); });
    probe.TextureCacheClear();
    tick([&] { check(probe.ResolveResourceCached(copy.c_str()) == first, "after texture clear"); });

    // 6. Per-tick maps still release at every tick end; ClearRetainedResources
    // (called by Destroy) releases the rest.
    tick([&] { probe.ResolveResourceRawCached(hash); });
    check(probe.mResolvedResourceCache.empty() && probe.mResolvedHashPaths.empty() && probe.mResolvedRawHashes.empty(),
          "per-tick release");
    probe.ClearRetainedResources();
    check(probe.mRetainedHashPaths.empty() && probe.mRetainedHashResources.empty() &&
              probe.mRetainedPathResources.empty() && !probe.mRetainedResourcesValid,
          "explicit release");

    std::ofstream("native-retained-resources.json")
        << "{\"passed\":true,\"checks\":" << checks << ",\"altAssets\":" << alt
        << ",\"hits\":" << probe.mRetainedResourceDiagnostics.hits
        << ",\"misses\":" << probe.mRetainedResourceDiagnostics.misses
        << ",\"clears\":" << probe.mRetainedResourceDiagnostics.clears << "}";
}
