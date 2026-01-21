/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#pragma once

#include "secure_storage.grpc.pb.h"
#include <gmock/gmock.h>

class SecureStorageServiceMock : public ::distp::gateway::secure_storage::v1::SecureStorageService::Service {
public:
    ~SecureStorageServiceMock() override = default;
    MOCK_METHOD(::grpc::Status, GetValue, (::grpc::ServerContext * context, const ::distp::gateway::secure_storage::v1::GetValueRequest* request, ::distp::gateway::secure_storage::v1::GetValueResponse* response), (override));
    MOCK_METHOD(::grpc::Status, UpdateValue, (::grpc::ServerContext * context, const ::distp::gateway::secure_storage::v1::UpdateValueRequest* request, ::distp::gateway::secure_storage::v1::UpdateValueResponse* response), (override));
    MOCK_METHOD(::grpc::Status, DeleteValue, (::grpc::ServerContext * context, const ::distp::gateway::secure_storage::v1::DeleteValueRequest* request, ::distp::gateway::secure_storage::v1::DeleteValueResponse* response), (override));
    MOCK_METHOD(::grpc::Status, DeleteAllValues, (::grpc::ServerContext * context, const ::distp::gateway::secure_storage::v1::DeleteAllValuesRequest* request, ::distp::gateway::secure_storage::v1::DeleteAllValuesResponse* response), (override));
    MOCK_METHOD(::grpc::Status, SeedValue, (::grpc::ServerContext * context, const ::distp::gateway::secure_storage::v1::SeedValueRequest* request, ::distp::gateway::secure_storage::v1::SeedValueResponse* response), (override));
};
