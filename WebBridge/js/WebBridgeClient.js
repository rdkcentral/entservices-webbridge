/**
 * Copyright 2023 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

const client = new WebSocket("ws://localhost:10001/jsonrpc", "json");
client.onopen = function(e) {
  console.log("client connected");

  let req = {};
  req.jsonrpc = "2.0"
  req.id = 10
  req.method = "org.rdk.Calculator.1.add";
  req.params = [ 2, 2 ]

  const s = JSON.stringify(req);
  console.log("send:" + s)
  client.send(s)
}
client.onmessage = function(msg) {
  const req = JSON.parse(msg.data)
  console.log("recv:" + JSON.stringify(req))
}
