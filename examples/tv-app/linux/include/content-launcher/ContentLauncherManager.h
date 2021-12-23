/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <app-common/zap-generated/af-structs.h>
#include <app/AttributeAccessInterface.h>
#include <app/clusters/content-launch-server/content-launch-delegate.h>

#include <lib/core/CHIPError.h>
#include <list>
#include <string>
#include <vector>

class ContentLauncherManager : public chip::app::Clusters::ContentLauncher::Delegate
{
public:
    ContentLauncherManager() : ContentLauncherManager({ "example", "example" }){};
    ContentLauncherManager(std::list<std::string> acceptHeaderList) :
        ContentLauncherManager(
            acceptHeaderList,
            //                               chip::app::Clusters::ContentLauncher::SupportedStreamingProtocol::kDash |
            static_cast<uint32_t>(chip::app::Clusters::ContentLauncher::SupportedStreamingProtocol::kHls)){};
    ContentLauncherManager(std::list<std::string> acceptHeaderList, uint32_t supportedStreamingProtocols);

    LaunchResponse HandleLaunchContent(chip::EndpointId endpointId, const std::list<Parameter> & parameterList, bool autoplay,
                                       const chip::CharSpan & data) override;
    LaunchResponse HandleLaunchUrl(const chip::CharSpan & contentUrl, const chip::CharSpan & displayString,
                                   const std::list<BrandingInformation> & brandingInformation) override;
    std::list<std::string> HandleGetAcceptHeaderList() override;
    uint32_t HandleGetSupportedStreamingProtocols() override;

protected:
    std::list<std::string> mAcceptHeaderList;
    uint32_t mSupportedStreamingProtocols;
};
