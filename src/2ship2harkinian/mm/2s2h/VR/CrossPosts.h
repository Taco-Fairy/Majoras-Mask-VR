#pragma once
#ifdef MMVR_ENABLE
#include <ship/resource/ResourceFactoryBinary.h>
namespace mmvrgame {
class CrossPostDisplayListFactory final : public Ship::ResourceFactoryBinary {
 public:
  std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
      std::shared_ptr<Ship::ResourceInitData> initData) override;
};
void VerifyCrossPosts();
}
#endif
