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

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

struct Server {
    std::unique_ptr<grpc::Server> server;
    Server(const std::string& uri, grpc::Service* service)
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(uri, grpc::InsecureServerCredentials());
        builder.RegisterService(service);
        server = builder.BuildAndStart();
    }
    ~Server()
    {
        server->Shutdown();
    }
};
