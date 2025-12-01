// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_TRANSLATE_VIVALDI_IOS_TH_SERVICE_FACTORY_H_
#define IOS_TRANSLATE_VIVALDI_IOS_TH_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace translate_history {
class TH_Model;
}

namespace translate_history {

class VivaldiIOSTHServiceFactory : public ProfileKeyedServiceFactoryIOS {
public:
  static TH_Model* GetForProfile(ProfileIOS* profile);
  static TH_Model* GetForProfileIfExists(ProfileIOS* profile);
  static VivaldiIOSTHServiceFactory* GetInstance();

private:
  friend class base::NoDestructor<VivaldiIOSTHServiceFactory>;

  VivaldiIOSTHServiceFactory();
  ~VivaldiIOSTHServiceFactory() override;
  VivaldiIOSTHServiceFactory(const VivaldiIOSTHServiceFactory&) = delete;
  VivaldiIOSTHServiceFactory& operator=(const VivaldiIOSTHServiceFactory&) = delete;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
       web::BrowserState* context) const override;
};

}  // namespace translate_history

#endif  // IOS_TRANSLATE_VIVALDI_IOS_TH_SERVICE_FACTORY_H_
