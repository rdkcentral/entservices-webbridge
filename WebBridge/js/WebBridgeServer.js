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

class CalculatorService extends Service {
  constructor() {
    super("org.rdk.Calculator");
    this.registerMethod('add', 1, this.add.bind(this));
    this.registerMethod('sub', 1, this.sub.bind(this));
  }

  add(params) {
    console.log("add(" + JSON.stringify(params) + ")")
    return new Promise((resolve, reject) => {
      let sum = params.reduce((a, b) => a + b, 0);
      resolve(sum)
    });
  }

  sub(params) {
    console.log("sub(" + JSON.stringify(params) + ")")
    return new Promise((resolve, reject) => {
      let diff = params[0] - params[1]
      resolve(diff)
    });
  }
}

const service_manager = new ServiceManager()
service_manager.open({"host":"127.0.0.1", "port":10001})
  .then(() => {
    service_manager.registerService(new CalculatorService())
  }).catch(ex => {
    console.log(ex.error);
  });
