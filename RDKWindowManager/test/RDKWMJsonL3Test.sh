#!/bin/sh

###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###

successCount=0
failuresCount=0
testAppTimeOut=15s

testHelp() {
    echo "Run Command to test the methods"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    echo "SI.No.  | TestCase             | Command                                  | Test Details"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "1" "testCreateDisplay1" "RDKWMJsonL3Test.sh testCreateDisplay1" "1. Without resolution, it will invoke the createDisplay function with the Westerosapp client."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2. It will invoke the getClients function to verify that the westerostest client is present."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3. The westerostest client will connect to the westeros_test app, which will then launch."
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "2" "testCreateDisplay2" "RDKWMJsonL3Test.sh testCreateDisplay2" "1. With given resolution, it will invoke the createDisplay function with the Westerosapp client."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2. It will invoke the getClients function to verify that the westerostest client is present."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3. The westerostest client will connect to the westeros_test app, which will then launch."
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "3" "testCreateDisplay3" "RDKWMJsonL3Test.sh testCreateDisplay3" "1. It will invoke the createDisplay function without any client."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2. Expecting ERROR_GENERAL in error code"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "4" "testGetClients1" "RDKWMJsonL3Test.sh testGetClients1" "1. It will invoke the getClients function without calling the createDisplay and ensure no clients are present"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "Note: This test was run without first performing any createDisplay."
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "5" "testGetClients2" "RDKWMJsonL3Test.sh testGetClients2" "1. It will invoke the createDisplay function with client only."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2. It will invoke the getClients function to verify that the client is present."
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "6" "testListeners" "RDKWMJsonL3Test.sh testListeners" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  AddKeyListener with keyCode 70 with client googletestapp"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  GenerateKey for keyCode 70 without client - f should be seen on the screen if focused is not none"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  RemoveKeyListener for keyCode 70 with client googletestapp"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  Deactivate webkitBrowser,internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "7" "testIntercept1" "RDKWMJsonL3Test.sh testIntercept1" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  AddKeyIntercept with keyCode 65 with shift with client googletestapp"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  InjectKey for keyCode 65 with Modifers shift - A should be seen on the google screen if focused is not none "
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  RemoveKeyIntercept with keyCode 65 with shift,client googletestapp"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  Deactivate webkitBrowser, internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "8" "testIntercept2" "RDKWMJsonL3Test.sh testIntercept2" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  AddKeyIntercept with keyCode 66 with client googletestapp"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  GenerateKey for keyCode 66 with shift,client googletestapp - B should be seen"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  RemoveKeyIntercept with keyCode 66 with client googletestapp"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  Deactivate webkitBrowser,internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "9" "testIntercept3" "RDKWMJsonL3Test.sh testIntercept3" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  Create and Launch westeros test"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  AddKeyIntercept with keyCode 67 with shift, client westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  InjectKey for keyCode 67 and 68 with shift  - D should be seen on the screen if focused is not none"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  RemoveKeyIntercept with keyCode 67 with shift,client westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "6.  kill westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "7.  Deactivate webkitBrowser, internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "10" "testIntercept4" "RDKWMJsonL3Test.sh testIntercept4" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  Create and Launch westeros test"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  AddKeyIntercept with keyCode 67 with shift, client westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  GenerateKey for keyCode 67 and 68 with shift ,without client - D should be seen on the screen if focused is not none"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  RemoveKeyIntercept with keyCode 67 with shift,client westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "6.  kill westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "7.  Deactivate webkitBrowser, internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "11" "testGenerateKey" "RDKWMJsonL3Test.sh testGenerateKey" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  Create and Launch westeros test with focus as true"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  GenerateKey for keyCode 67 and 68 without client specified - C and D should not be seen"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  GenerateKey for keyCode 67 and 68 with client specified - C and D should be seen on the screen"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  kill westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "6.  Deactivate webkitBrowser, internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "12" "testIntercepts" "RDKWMJsonL3Test.sh testIntercepts" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  Create and Launch westeros test"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  AddKeyIntercepts for keyCode 75 and 76 with shift,client googletestapp & 80 with shift and 79 with no modifiers,client westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  GenerateKey for keyCode 79 with no modifiers and 76 with shift, client - o and L should be seen on the screen"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5.  GenerateKey for keyCode 79 with no modifiers and 76 with shift, without client - L should be seen on the screen"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "6.  RemoveKeyIntercept for keyCode 79 with client westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "7.  kill westerostest"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "8.  Deactivate webkitBrowser, internally kills the client googletestapp"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "13" "testInjectGenerateKey" "RDKWMJsonL3Test.sh testInjectGenerateKey" "1. Create and Launch GoogleBrowser with webkitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2.  injectKey for keyCode 71 - G should be seen if focused is not none  "
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3.  GenerateKey for keyCode 72 and 73 with shift,client - h and I should be seen"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4.  Deactivate webkitBrowser internally kill the client"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "14" "testKeyListenersFailures" "RDKWMJsonL3Test.sh testKeyListenersFailures" "1. AddKeyListener with both keyCode and NativeKeyCode present"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "2. Expecting ERROR_GENERAL in error code"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "3. AddKeyListener without both keyCode and NativeKeyCode"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "4. Expecting ERROR_GENERAL in error code"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "5. RemoveKeyListener with both keyCode and NativeKeyCode"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "6. Expecting ERROR_GENERAL in error code"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "7. RemoveKeyListener without both keyCode and NativeKeyCode"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "" "8. Expecting ERROR_GENERAL in error code"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "15" "testInactivity1" "RDKWMJsonL3Test.sh testInactivity1" "1. Enable the inactivity reporting feature."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.  Set inactivity time of 1minute."
    echo "---------------------|------------------------------------------|--------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "16" "testInactivity2" "RDKWMJsonL3Test.sh testInactivity2" "1. Disable inactivity reportingfeature."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2. Set inactivity time of 1minute."
    echo "---------------------|------------------------------------------|--------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "17" "testInactivity3" "RDKWMJsonL3Test.sh testInactivity3" "1. Enable Inactivity feature."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Set inactivity time interval of 1minute"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Wait for 40sec,Reset the inactivity."
    echo "---------------------|------------------------------------------|--------------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "18" "testInactivity4" "RDKWMJsonL3Test.sh testInactivity4" "1. Enable Inactivity reporting feature."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2. Set inactivity time of 1min and wait for 40secs."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3. Inject key"
    echo "--------|----------------------|------------------------------------------|-------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "19" "testGetKeyRepeatsEnabled" "RDKWMJsonL3Test.sh testGetKeyRepeatsEnabled" "1. Call the testEnableKeyRepeats function to disable the key repeat feature."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.invoke the org.rdk.RDKWindowManager.getKeyRepeatsEnabled method "
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Verify that the response is false, indicating that key repeats are disabled."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "4.Call the testEnableKeyRepeats function to enable the key repeat feature"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "5.Invoke the org.rdk.RDKWindowManager.getKeyRepeatsEnabled method "
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "6.Verify that the response is true, confirming that key repeats are enabled"
    echo "--------|----------------------|------------------------------------------|---------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "20" "testInputEventEnable" "RDKWMJsonL3Test.sh testInputEventEnable" "1. create a display using the createDisplay API."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Activate the WebKitBrowser plugin by invoking the Controller.1.activate"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Load the Google homepage in the WebKitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "4.Enable input events for the testapp client by calling the testSetInputEventEnable function."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "5.Attempt to inject a key using the injectKey API"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "6.Verify that the response is null"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "7.Confirm that the key injection is  accepted on the WebKitBrowser application side when input events are Enabled"
    echo "--------|----------------------|------------------------------------------|--------------------------------------------------------------------"
	printf "%-7s | %-20s | %-40s | %-70s\n" "21" "testInputEventDisble" "RDKWMJsonL3Test.sh testInputEventDisable" "1. create a display using the createDisplay API."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Activate the WebKitBrowser plugin by invoking the Controller.1.activate"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Load the Google homepage in the WebKitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "4.Disable input events for the testapp client by calling the testSetInputEventDisable function."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "5.Attempt to inject a key using the injectKey API"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "6.Verify that the response is nul"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "7.Confirm that the key injection is NOT accepted on the WebKitBrowser application side when input events are Enabled"
	echo "--------|----------------------|------------------------------------------|--------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "22" "testIgnoreKeyInputsEnable" "RDKWMJsonL3Test.sh testIgnoreKeyInputsEnable" "The RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE feature must be enabled (set to 1) for this function to execute."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "1.Create a display using the createDisplay API."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Activate the WebKitBrowser plugin by invoking the Controller.1.activate"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Load the Google homepage in the WebKitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "4.Enable ignore key input  for the testapp client by calling the IgnoreKeyInputs function."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "5.Attempt to press a key using the Remote Controller"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "6.Confirm that the key press is NOT accepted on the WebKitBrowser application side when IgnoreKeyInput is Enabled"
    echo "--------|----------------------|------------------------------------------|----------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "23" "testIgnoreKeyInputsDisable" "RDKWMJsonL3Test.sh testIgnoreKeyInputsDisable" "The RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE feature must be enabled (set to 1) for this function to execute."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "1.Create a display using the createDisplay API."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Activate the WebKitBrowser plugin by invoking the Controller.1.activate"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Load the Google homepage in the WebKitBrowser"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "4.Disable ignore key input  for the testapp client by calling the IgnoreKeyInputs function."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "5.Attempt to press a key using the Remote Controller"
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "6.Confirm that the key press is accepted on the WebKitBrowser application side when IgnoreKeyInput is Disabled"
    echo "--------|----------------------|------------------------------------------|----------------------------------------------------------------------"     printf "%-7s | %-20s | %-40s | %-70s\n" "24" "testKeyRepeatConfigUnsupportedInput" "RDKWMJsonL3Test.sh testKeyRepeatConfigUnsupportedInput" "1. Attempt to configure keyRepeatConfig for an unsupported input type (e.g., "mouse") using the API.."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Verify that the response contains an error with code: 1 and message: "ERROR_GENERAL"."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Confirm that the key repeat configuration is not applied due to the missing parameter."
    echo "--------|----------------------|------------------------------------------|------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "25" "testKeyRepeatConfigMissingEnabled" "RDKWMJsonL3Test.sh testKeyRepeatConfigMissingEnabled" "1. Invoke the keyRepeatConfig API for the "default" input, omitting the enabled parameter."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "2.Verify that the response contains an error with code: 1 and message: "ERROR_GENERAL"."
    printf "%-7s | %-20s | %-40s | %-70s\n" "" "" "3.Confirm that the key repeat configuration is not applied due to the missing parameter."
    echo "--------|----------------------|------------------------------------------|---------------------------------------------------------------------"
    printf "%-7s | %-20s | %-40s | %-70s\n" "26" "testAll" "RDKWMJsonL3Test.sh testAll" "1. Run all the tests"
    echo "---------------------|------------------------------------------|--------------------------------------------------------------------------"
}

#killWesterosTestApp
killWesterosTestApp()
{
    PID=$(ps -eo pid,comm | grep "westeros_test" | grep -v "grep" | awk '{print $1}')
    kill ${PID}
}

#createDisplayAndLaunchGoogleBrowserWithWebkitBrowser
createDisplayAndLaunchGoogleBrowser()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"googletestapp\", \"displayName\": \"wst-webkitbrowser\"}"}}'
    expectedResult='null'
    testAPI "CreateDisplayForGoogle" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "WebKitBrowser"}}'
    expectedResult='null'
    testAPI "WebkitBrowserActivate" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc":"2.0","id":"3","method":"WebKitBrowser.1.url","params":"http://www.google.com"}'
    expectedResult='null'
    testAPI "LaunchGoogleWeb" "$inputParams" "$expectedResult"
}

#deactivateWebKitBrowser
deactivateWebKitBrowser()
{
    inputParams='{"jsonrpc": "2.0","id": 4,"method": "Controller.1.deactivate", "params": { "callsign": "WebKitBrowser"}}'
    expectedResult='null'
    testAPI "WebkitBrowserDeactivate" "$inputParams" "$expectedResult"
}

if [ "$1" = "help" ]; then
  testHelp
  exit 0
fi

isThunderSecurityEnabled="0"
command -v tr181 >/dev/null 2>&1
if [ $? -eq 0 ];then
  echo "checking thunder security"
  THUNDER_SECURITY_ENABLED=`tr181 -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ThunderSecurity.Enable 2>&1`
  if [ $THUNDER_SECURITY_ENABLED = "true" ]; then
    echo "Thunder security is enabled"
    isThunderSecurityEnabled="1"
  else
    echo "Thunder security is disabled"
  fi
else
  echo "command tr181 not found "
fi

securityToken=""
if [ $isThunderSecurityEnabled = "1" ]; then
  securityToken=`WPEFrameworkSecurityUtility |awk -F\" '{print $4}'`
fi

validate()
{
  api=$1
  expectedResult=$2
  curlOutput=$3
  inputparams=$4

  if echo "$curlOutput" | grep -q '"result":'; then
      resultParam=$(echo "$curlOutput" | awk -F'"result":' '{printf $2}' | sed "s/[{}]//g" | awk -F',' '{print $0}')
      echo "Success: $resultParam"
  elif echo "$curlOutput" | grep -q '"error":'; then
      resultParam=$(echo "$curlOutput" | awk -F'"error":' '{printf $2}' | sed "s/[{}]//g" | awk -F',' '{print $0}')
      echo "Error: $resultParam"
  else
      echo "Neither result nor error found."
  fi
  if [ "$resultParam" = "$expectedResult" ]; then
    successCount=$((successCount+1))
    echo "---------------------------------------------------------------"
    printf "Request: %s\n" "$inputParams"
    printf "Expected: %s\n" "$expectedResult"
    printf "Result: %s\n" "$resultParam"
    echo "$api : Success"
    echo "---------------------------------------------------------------"
    return 0
  else
    failuresCount=$((failuresCount+1))
    echo "---------------------------------------------------------------"
    printf "Request: %s\n" "$inputParams"
    printf "Expected: %s\n" "$expectedResult"
    printf "Result: %s\n" "$resultParam"
    echo "$api : Failed - [$resultParam] [$expectedResult]"
    echo "---------------------------------------------------------------"
    return 1
  fi
}

#executes command and validates the output
testAPI()
{
  apiname=$1
  inputparams=$2
  expectedResult=$3

  params=`echo $inputparams`
  curl_command=""
  if [ $isThunderSecurityEnabled = "1" ]
  then
    token=`echo $securityToken`
    curl_command="curl -H \"Content-Type: application/json\"  -H \"Authorization: Bearer $token\" -X POST --silent -d '$params' http://127.0.0.1:9998/jsonrpc"
  else
    curl_command="curl --silent -d '$params' http://127.0.0.1:9998/jsonrpc"
  fi
  curlOutput=`eval $curl_command`
  validate $apiname $expectedResult "$curlOutput" $inputparams
}

# Main logic to run the user-specified function

if [ $# -eq 0 ];then
    echo "No function name provided "
    exit 1
fi

function_name=$1

#testCreateDisplayAndGetClientsForWesterosTestAppWithOutResolution
testCreateDisplay1()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams": "{\"client\":\"westerosapp\",\"displayName\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "testCreateDisplay1" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": 42,"method": "org.rdk.RDKWindowManager.getClients"}'
    expectedResult='"[\"westerosapp\"]"'
    testAPI "testCreateDisplay1" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    /usr/bin/westeros_test --display westerostest &

    sleep $testAppTimeOut #15secs wait then run next test as it's westeros test app test, need to view in display
    killWesterosTestApp #kill westeros test app if its running
    sleep 2s
}

#testCreateDisplayAndGetClientsForWesterosTestAppWithResolution
testCreateDisplay2()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"westerostest1\", \"displayName\": \"test1\",\"displayWidth\": 1920,\"displayHeight\": 1080,\"virtualDisplay\": false,\"virtualWidth\": 1920,\"virtualHeight\": 1080,\"topmost\": false,\"focus\": false}"}}'
    expectedResult='null'
    testAPI "testCreateDisplay2" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": 42,"method": "org.rdk.RDKWindowManager.getClients"}'
    expectedResult='"[\"westerostest1\"]"'
    testAPI "testCreateDisplay2" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    export WAYLAND_DISPLAY=test1
    /usr/bin/westeros_test &

    sleep $testAppTimeOut #15secs wait then run next test as it's westeros test app test, need to view in display
    killWesterosTestApp #kill westeros test app if its running
    sleep 2s
}

#testCreateDisplayWithOutClient
testCreateDisplay3()
{
    inputParams='{"jsonrpc": "2.0","id": 42,"method": "org.rdk.RDKWindowManager.createDisplay","params": {"displayParams": "{\"displayName\": \"test\"}"}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testCreateDisplay3" "$inputParams" "$expectedResult"
}

#testListenersWithGenerateKey
testListeners()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc": "2.0","id": "4","method": "org.rdk.RDKWindowManager.addKeyListener","params": {"keyListeners": "{\"keys\": [{\"keyCode\":70, \"activate\":true, \"propagate\":false}], \"client\": \"googletestapp\"}"}}'
    expectedResult='null'
    testAPI "addKeyListener" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 70, \"delay\": 5, \"modifiers\": []}]}"}}'
    expectedResult='null'
    testAPI "generateKeyafterAddKeyListener" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "4","method": "org.rdk.RDKWindowManager.removeKeyListener","params": {"keyListeners": "{\"keys\": [{\"keyCode\":70, \"activate\":true, \"propagate\":false}], \"client\": \"googletestapp\"}"}}'
    expectedResult='null'
    testAPI "removeKeyListener" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    deactivateWebKitBrowser

    sleep $testAppTimeOut
}

#testInterceptWithInjectKey
testIntercept1()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.addKeyIntercept", "params":{"intercept":"{\"keyCode\":65,\"modifiers\": [\"shift\"],\"client\":\"googletestapp\"}"}}'
    expectedResult='null'
    testAPI "addKeyIntercept" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.injectKey","params": {"keyCode":65, "modifiers": "{\"modifiers\": [\"shift\"]}"}}'
    expectedResult='null'
    testAPI "injectKeyAfterAddKeyIntercept" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.removeKeyIntercept", "params":{"intercept":"{\"keyCode\":65,\"modifiers\": [\"shift\"],\"client\":\"googletestapp\"}"}}'
    expectedResult='null'
    testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    deactivateWebKitBrowser

    sleep $testAppTimeOut

}

#testInterceptWithGenerateKeyClientSpecified
testIntercept2()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.addKeyIntercept", "params":{"intercept":"{\"keyCode\":66,\"modifiers\": [\"shift\"],\"client\":\"googletestapp\"}"}}'
    expectedResult='null'
    testAPI "addKeyIntercept" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 66, \"delay\": 10, \"modifiers\": [\"shift\"]}]}"}}'
    expectedResult='null'
    testAPI "generateKeyAfterAddKeyInterceptForGoogletest" "$inputParams" "$expectedResult"

    sleep 25s

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.removeKeyIntercept", "params":{"intercept":"{\"keyCode\":66,\"modifiers\": [\"shift\"],\"client\":\"googletestapp\"}"}}'
    expectedResult='null'
    testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    deactivateWebKitBrowser

    sleep $testAppTimeOut

}

#testInterceptWithMutipleDisplayInjectKey
testIntercept3()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "westerostestapp" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    export WAYLAND_DISPLAY=westerostest
    /usr/bin/westeros_test &

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.addKeyIntercept", "params":{"intercept":"{\"keyCode\":67,\"modifiers\": [\"shift\"],\"client\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "addKeyIntercept" "$inputParams" "$expectedResult"

    keyCodes=(67 68)

    for keyCode in "${keyCodes[@]}"; do
        inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.injectKey","params": {"keyCode":'"$keyCode"', "modifiers": "{\"modifiers\": [\"shift\"]}"}}'
        testName="injectKeyAfterAddKeyInterceptKeyCode$keyCode"
        expectedResult='null'
        testAPI "$testName" "$inputParams" "$expectedResult"
        sleep 2s
    done

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.removeKeyIntercept", "params":{"intercept":"{\"keyCode\":67,\"modifiers\": [\"shift\"],\"client\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    killWesterosTestApp #kill westeros test app if its running

    deactivateWebKitBrowser

    sleep $testAppTimeOut

}

#testInterceptWithMutipleDisplayGenerateKey
testIntercept4()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "westerostestapp" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    export WAYLAND_DISPLAY=westerostest
    /usr/bin/westeros_test &

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.addKeyIntercept", "params":{"intercept":"{\"keyCode\":67,\"modifiers\": [\"shift\"],\"client\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "addKeyIntercept" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 67, \"delay\": 5, \"modifiers\": [\"shift\"]},{\"keyCode\": 68, \"delay\": 5, \"modifiers\": [\"shift\"]}]}"}}'
    expectedResult='null'
    testAPI "generateKeyAfterAddKeyInterceptWithoutClientSpecified" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.removeKeyIntercept", "params":{"intercept":"{\"keyCode\":67,\"modifiers\": [\"shift\"],\"client\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

    killWesterosTestApp #kill westeros test app if its running

    deactivateWebKitBrowser

    sleep $testAppTimeOut
}

#testGenerateKeyWithwesterosasfocusClientSpecifiedAndNotSpecified
testGenerateKey()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "westerostestapp" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    export WAYLAND_DISPLAY=westerostest
    /usr/bin/westeros_test &

    sleep $testAppTimeOut

    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 67, \"delay\": 5, \"modifiers\": [\"shift\"]},{\"keyCode\": 68, \"delay\": 5, \"modifiers\": [\"shift\"]}]}"}}'
    expectedResult='null'
    testAPI "generateKeyAfterAddKeyInterceptWithoutClient" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 67, \"delay\": 5, \"modifiers\": [\"shift\"]},{\"keyCode\": 68, \"delay\": 5, \"modifiers\": [\"shift\"]}]}","client": "googletestapp"}}'
    expectedResult='null'
    testAPI "generateKeyAfterAddKeyInterceptWithClient" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    killWesterosTestApp #kill westeros test app if its running

    deactivateWebKitBrowser

    sleep $testAppTimeOut
}

testIntercepts()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "westerostestapp" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    export WAYLAND_DISPLAY=westerostest
    /usr/bin/westeros_test &

    sleep $testAppTimeOut

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.addKeyIntercepts","params": {"intercepts": "{\"intercepts\": [{\"keys\": [{\"keyCode\": 75, \"modifiers\": [\"shift\"]},{\"keyCode\": 76, \"modifiers\": [\"shift\"]}],\"client\": \"googletestapp\"},{\"keys\": [{\"keyCode\": 79, \"modifiers\": []},{\"keyCode\": 80, \"modifiers\": [\"shift\"]}],\"client\": \"westerostest\"}]}"}}'
    expectedResult='null'
    testAPI "addKeyInterceptsForbothWesterostestandgoogletest" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 79, \"delay\": 5, \"modifiers\": []},{\"keyCode\": 76, \"delay\": 5, \"modifiers\": [\"shift\"]}]}","client": "googletestapp"}}'
    expectedResult='null'
    testAPI "generateKeyAfterAddKeyInterceptsForGoogletestapp" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 79, \"delay\": 5, \"modifiers\": []},{\"keyCode\": 76, \"delay\": 5, \"modifiers\": [\"shift\"]}]}"}}'
    expectedResult='null'
    testAPI "generateKeyAfterAddKeyInterceptsWithoutClient" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.removeKeyIntercept", "params":{"intercept":"{\"keyCode\":79,\"modifiers\": [],\"client\":\"westerostest\"}"}}'
    expectedResult='null'
    testAPI "removeKeyIntercept" "$inputParams" "$expectedResult"

    killWesterosTestApp #kill westeros test app if its running

    deactivateWebKitBrowser

    sleep $testAppTimeOut
}

#testInjectKeyAndGenerateKey
testInjectGenerateKey()
{
    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    sleep $testAppTimeOut

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.injectKey","params": {"keyCode":71, "modifiers": "{\"modifiers\": [\"shift\"]}"}}'
    expectedResult='null'
    testAPI "injectKey" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.generateKey","params": {"keys": "{\"keys\": [{\"keyCode\": 72, \"delay\": 5, \"modifiers\": []},{\"keyCode\": 73, \"delay\": 5, \"modifiers\": [\"shift\"]}]}","client": "googletestapp"}}'
    expectedResult='null'
    testAPI "generateKey" "$inputParams" "$expectedResult"

    sleep $testAppTimeOut

    deactivateWebKitBrowser

    sleep $testAppTimeOut
}

#testAddKeyListenersAndRemoveKeyListenersForFailureCases
testKeyListenersFailures()
{
    inputParams='{"jsonrpc": "2.0","id": "4","method": "org.rdk.RDKWindowManager.addKeyListener","params": {"keyListeners": "{\"keys\": [{\"keyCode\":70,\"nativeKeyCode\": 72,\"activate\":true, \"propagate\":false}], \"client\": \"googletestapp\"}"}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testAddKeyListenersForFailureCasesForAddingBothKeyCodeAndNativeKeyCode" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "4","method": "org.rdk.RDKWindowManager.addKeyListener","params": {"keyListeners": "{\"keys\": [{\"activate\":true, \"propagate\":false}], \"client\": \"googletestapp\"}"}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testAddKeyListenersForFailureCasesForNotHavingBothKeyCodeAndNativeKeyCode" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "4","method": "org.rdk.RDKWindowManager.removeKeyListener","params": {"keyListeners": "{\"keys\": [{\"keyCode\":70,\"nativeKeyCode\": 72,\"activate\":true, \"propagate\":false}], \"client\": \"googletestapp\"}"}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testRemoveKeyListenersForFailureCasesForAddingBothKeyCodeAndNativeKeyCode" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": "4","method": "org.rdk.RDKWindowManager.removeKeyListener","params": {"keyListeners": "{\"keys\": [{\"activate\":true, \"propagate\":false}], \"client\": \"googletestapp\"}"}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testRemoveKeyListenersForFailureCasesForNotHavingBothKeyCodeAndNativeKeyCode" "$inputParams" "$expectedResult"
}

#testGetClientsWithOutCreateDisplay
testGetClients1()
{
    inputParams='{"jsonrpc": "2.0","id": 42,"method": "org.rdk.RDKWindowManager.getClients"}'
    expectedResult='""'
    testAPI "testGetClients2" "$inputParams" "$expectedResult"
}

#testGetClientsWithCreateDisplay
testGetClients2()
{
    inputParams='{"jsonrpc": "2.0","id": 42,"method": "org.rdk.RDKWindowManager.createDisplay","params": {"displayParams": "{\"client\": \"org.rdk.Netflix\",\"callsign\": \"org.rdk.Netflix\",\"displayName\": \"test\"}"}}'
    expectedResult='null'
    testAPI "testGetClients1" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc": "2.0","id": 42,"method": "org.rdk.RDKWindowManager.getClients"}'
    expectedResult='"[\"org.rdk.netflix\"]"'
    testAPI "testGetClients1" "$inputParams" "$expectedResult"
}

#Enable the feature and Set inactivity time of 1minute
testInactivity1()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.enableInactivityReporting","params":{"enable": true}}'
    expectedResult='null'
    testAPI "enableInactivityReporting" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.setInactivityInterval","params":{"interval":1}}'
    expectedResult='null'
    testAPI "setInactivityInterval" "$inputParams" "$expectedResult"
    sleep 2m
}

#Disable inactivity feature & set inactivity time of 1minute
testInactivity2()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.enableInactivityReporting","params":{"enable": false}}'
    expectedResult='null'
    testAPI "enableInactivityReporting" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.setInactivityInterval","params":{"interval":1}}'
    expectedResult='null'
    testAPI "setInactivityInterval" "$inputParams" "$expectedResult"
    sleep 2m
}

#Enable inactivity feature,wait for 1 min,when it goes nears 1min,40sec,reset the inactivity,onuserinactive should happen after a min frm current time
testInactivity3()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.enableInactivityReporting","params":{"enable": true}}'
    expectedResult='null'
    testAPI "enableInactivityReporting" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.setInactivityInterval","params":{"interval":1}}'
    expectedResult='null'
    testAPI "setInactivityInterval" "$inputParams" "$expectedResult"

    sleep 40

    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.resetInactivityTime"}}'
    expectedResult='null'
    testAPI "resetInactivityTime" "$inputParams" "$expectedResult"
    sleep 2m
}

#Enable inactivity feature,wait for 1 min,when it goes near 1min,say 40sec,inject key,
#onuserinactive should happen after a min frm current time"
testInactivity4()
{
    inputParams='{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams":"{\"callsign\":\"westerostest1\", \"displayName\": \"test1\",\"displayWidth\": 1920,\"displayHeight\": 1080,\"virtualDisplay\": false,\"virtualWidth\": 1920,\"virtualHeight\": 1080,\"topmost\": false,\"focus\": false}"}}'
    expectedResult='null'
    testAPI "testCreateDisplay2" "$inputParams" "$expectedResult"

    export XDG_RUNTIME_DIR=/tmp
    export WAYLAND_DISPLAY=test1
    /usr/bin/westeros_test &

    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.enableInactivityReporting","params":{"enable": true}}'
    expectedResult='null'
    testAPI "enableInactivityReporting" "$inputParams" "$expectedResult"

    inputParams='{"jsonrpc":"2.0","id":"3","method":"org.rdk.RDKWindowManager.setInactivityInterval","params":{"interval":1}}'
    expectedResult='null'
    testAPI "setInactivityInterval" "$inputParams" "$expectedResult"

    sleep 40

    inputParams='{"jsonrpc": "2.0","id": "3","method": "org.rdk.RDKWindowManager.injectKey","params": {"keyCode":66, "modifiers": "{\"modifiers\": [\"shift\"]}"}}'
    expectedResult='null'
    testAPI "injectKey" "$inputParams" "$expectedResult"
    sleep 2m
    killWesterosTestApp #kill westeros test app if its running
    sleep 2s
}

# Test EnableKeyRepeats
testEnableKeyRepeats() {
    enable=$1  # "true" or "false"
    inputParams="{\"jsonrpc\": \"2.0\", \"id\": \"1\", \"method\": \"org.rdk.RDKWindowManager.enableKeyRepeats\", \"params\": {\"enable\": $enable}}"
    expectedResult='null'
    testAPI "testEnableKeyRepeat_$enable" "$inputParams" "$expectedResult"
}

# Test GetKeyRepeats Enabled state
testGetKeyRepeatsEnabled() {
    echo "---- Testing GetKeyRepeatsEnabled Flow ----"

    # Disable Key Repeat
    testEnableKeyRepeats "false"

    inputParams='{"jsonrpc": "2.0", "id": "3", "method": "org.rdk.RDKWindowManager.getKeyRepeatsEnabled"}'
    expectedResult='false'
    testAPI "testGetKeyRepeatsEnabled" "$inputParams" "$expectedResult"

    #Enable Key Repeat
    testEnableKeyRepeats "true"

    inputParams='{"jsonrpc": "2.0", "id": "3", "method": "org.rdk.RDKWindowManager.getKeyRepeatsEnabled"}'
    expectedResult='true'
    testAPI "testGetKeyRepeatsEnabled" "$inputParams" "$expectedResult"
}

# Test EnableInputEvents - Enable
testSetInputEventEnable() {
    client_name="$1"  # Accept client name as parameter

    inputParams='{
        "jsonrpc": "2.0",
        "id": "5",
        "method": "org.rdk.RDKWindowManager.enableInputEvents",
        "params": { "clients": {"clients": ["'"$client_name"'"]}, "enable": true }}'

    expectedResult='null'
    testAPI "testSetInputEventEnable" "$inputParams" "$expectedResult"
}

# Test EnableInputEvents - Disable
testSetInputEventDisable() {
    client_name="$1"  # Accept client name as parameter

    inputParams='{
        "jsonrpc": "2.0",
        "id": "6",
        "method": "org.rdk.RDKWindowManager.enableInputEvents",
        "params": { "clients": {"clients": ["'"$client_name"'"]}, "enable": false }}'

    expectedResult='null'
    testAPI "testSetInputEventDisable" "$inputParams" "$expectedResult"
}

# Test Generate Key when Input Events are Enabled (Confirm it is accepted the key)
testInputEventEnable() {
    client_name="testapp"
    #CreateDisplay
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`"  --header "Content-Type: application/json" --request POST -d  '{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams": "{\"client\":\"testapp\",\"displayName\":\"wst-webkitbrowser\"}"}}' http://127.0.0.1:9998/jsonrpc;
    #webkitBrower activate
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`"  --header "Content-Type: application/json" --request POST -d  '{"jsonrpc": "2.0","id": 5,"method": "Controller.1.activate", "params": { "callsign": "WebKitBrowser"}}' http://127.0.0.1:9998/jsonrpc ;
    #Load Google Browser:
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d '{"jsonrpc":"2.0","id":"3","method":"WebKitBrowser.1.url","params":"http://www.google.com"}' http://127.0.0.1:9998/jsonrpc;

    # Enable Input Events
    testSetInputEventEnable "$client_name"

    sleep $testAppTimeOut

    injectKeyInput='{"jsonrpc": "2.0", "id": "7", "method": "org.rdk.RDKWindowManager.injectKey", "params": {"keyCode":79, "modifiers": "{\"modifiers\": []}"}}'
    expectedResult='null'
    testAPI "testInjectKey_InputEventEnabled" "$injectKeyInput" "$expectedResult"

    sleep $testAppTimeOut
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d  '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.deactivate", "params": { "callsign": "WebKitBrowser"}}' http://127.0.0.1:9998/jsonrpc;

    echo "---- Confirm on the WebKitBrowser application side that the Generate key is  accepted when input events are Enabled. ----"
}

# Test Generate Key when Input Events are Disabled (Confirm it do not accept the key)
testInputEventDisable() {
    client_name="testapp"
    #CreateDisplay
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`"  --header "Content-Type: application/json" --request POST -d  '{"jsonrpc":"2.0","id":"3","method": "org.rdk.RDKWindowManager.createDisplay", "params":{"displayParams": "{\"client\":\"testapp\",\"displayName\":\"wst-webkitbrowser\"}"}}' http://127.0.0.1:9998/jsonrpc;
    #webkitBrower actiate
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`"  --header "Content-Type: application/json" --request POST -d  '{"jsonrpc": "2.0","id": 5,"method": "Controller.1.activate", "params": { "callsign": "WebKitBrowser"}}' http://127.0.0.1:9998/jsonrpc ;
    #Load Google Browser:
    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d '{"jsonrpc":"2.0","id":"3","method":"WebKitBrowser.1.url","params":"http://www.google.com"}' http://127.0.0.1:9998/jsonrpc; 

    # Disable Input Events
    testSetInputEventDisable "$client_name"
    sleep $testAppTimeOut

    injectKeyInput='{"jsonrpc": "2.0", "id": "7", "method": "org.rdk.RDKWindowManager.injectKey", "params": {"keyCode":79, "modifiers": "{\"modifiers\": []}"}}'
    expectedResult='null'
    testAPI "testInjectKey_InputEventDisabled" "$injectKeyInput" "$expectedResult"
    sleep $testAppTimeOut

    curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST -d  '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.deactivate", "params": { "callsign": "WebKitBrowser"}}' http://127.0.0.1:9998/jsonrpc;
    echo "---- Confirm on the WebKitBrowser application side that the Generate key is NOT accepted when input events are disabled. ----"
}


# Test IgnoreKeyInputs (Enable/Disable based on argument)
testSetIgnoreKeyInputs() {

# Check if RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is 1 and run the tests
if [ "$RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE" = "1" ]; then
    echo "Environment variable RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is set to 1. Running the tests.."

    state="$1"  # Accept the state (true or false) as argument

    inputParams='{
        "jsonrpc": "2.0",
        "id": "5",
        "method": "org.rdk.RDKWindowManager.ignoreKeyInputs",
        "params": {"ignore": '"$state"'}
    }'
    expectedResult='null'
    testAPI "testIgnoreKeyInputs_$state" "$inputParams" "$expectedResult"
else
    echo "Environment variable RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is NOT set. Skipping the tests."
fi
}

# Test Key Press when Ignore Key input is Enabled (Confirm it do not accept the key)
testIgnoreKeyInputsEnable() {

# Check if RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is 1 and run the tests
if [ "$RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE" = "1" ]; then
    echo "Environment variable RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is set to 1. Running the tests.."

    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser

    echo "---- Testing Inject Key Flow when Input Events are Disabled ----"
    # Enable IgnoreKey Input
    testSetIgnoreKeyInputs "true"

    sleep $testAppTimeOut

    # Verify that key is NOT displayed on the screen
    echo "---- Confirm on the WebKitBrowser application side that the physical key press [remote key press] is NOT  accepted when IgnoreKey Inputs Disbled. ----"

    sleep $testAppTimeOut

    deactivateWebKitBrowser

    sleep $testAppTimeOut
else
    echo "Environment variable RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is NOT set. Skipping the tests."
fi
}

# Test Key press when Ignore Key input is Disabled (Confirm it accept the key)
testIgnoreKeyInputsDisable() {

# Check if RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is 1 and run the tests
if [ "$RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE" = "1" ]; then
    echo "Environment variable RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is set to 1. Running the tests.."

    createDisplayAndLaunchGoogleBrowser   #Launch the GoogleBrowser with webkitBrowser
    echo "---- Testing Inject Key Flow when Input Events are Disabled ----"
    # Disable Ignore KeyInput
    testSetIgnoreKeyInputs "false"

    sleep $testAppTimeOut

    # Verify that the key is displayed on the screen

    echo "---- Confirm on the WebKitBrowser application side that the physical key press [remote key press] is  accepted when IgnoreKey Inputs Disbled. ----"

    sleep $testAppTimeOut

    deactivateWebKitBrowser

    sleep $testAppTimeOut
else
    echo "Environment variable RDK_WINDOW_MANAGER_ENABLE_KEY_IGNORE is NOT set. Skipping the tests."
fi

}


#Test KeyRepeatConfig enable/disable
testSetKeyRepeatConfig() {
    enabled="$1"  # Accept 'enabled' state (true/false) as argument
    inputParams='{"jsonrpc": "2.0", "id": "1", "method": "org.rdk.RDKWindowManager.keyRepeatConfig", "params": {"input": "default", "keyConfig": {"enabled": '"$enabled"', "initialDelay": 500, "repeatInterval": 100}}}'
    expectedResult='null'
    testAPI "testSetKeyRepeatConfig" "$inputParams" "$expectedResult"
}


#Test KeyRepeatConfig Unsupported input [any input other than "default"/"keyboard" considered as unsupported]
testKeyRepeatConfigUnsupportedInput() {
    inputParams='{"jsonrpc": "2.0", "id": "5", "method": "org.rdk.RDKWindowManager.keyRepeatConfig", "params": {"input": "mouse", "keyConfig": {"enabled": true, "initialDelay": 500, "repeatInterval": 100}}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testKeyRepeatConfig_UnsupportedInput" "$inputParams" "$expectedResult"
}

#Test misisng enabled param
testKeyRepeatConfigMissingEnabled() {
    inputParams='{"jsonrpc": "2.0", "id": "6", "method": "org.rdk.RDKWindowManager.keyRepeatConfig", "params": {"input": "default", "keyConfig": {"initialDelay": 500, "repeatInterval": 100}}}'
    expectedResult='"code":1,"message":"ERROR_GENERAL"'
    testAPI "testKeyRepeatConfig_MissingEnabled" "$inputParams" "$expectedResult"
}

#testAll
testAll()
{
    echo "---------------------------------------------------------------"
    echo "Running testCreateDisplay1"
    testCreateDisplay1
    echo "wait for 10secs and start next test"
    echo "Completed testCreateDisplay1"
    echo "---------------------------------------------------------------"
    echo "Running testCreateDisplay2"
    testCreateDisplay2
    echo "Completed testCreateDisplay2"
    echo "---------------------------------------------------------------"
    echo "Running testCreateDisplay3"
    testCreateDisplay3
    echo "Completed testCreateDisplay3"
    echo "---------------------------------------------------------------"
    echo "Running testListeners"
    testListeners
    echo "Completed testListeners"
    echo "---------------------------------------------------------------"
    echo "Running testIntercept1"
    testIntercept1
    echo "Completed testIntercept1"
    echo "---------------------------------------------------------------"
    echo "Running testIntercept2"
    testIntercept2
    echo "Completed testIntercept2"
    echo "---------------------------------------------------------------"
    echo "Running testIntercept3"
    testIntercept3
    echo "Completed testIntercept3"
    echo "---------------------------------------------------------------"
    echo "Running testIntercept4"
    testIntercept4
    echo "Completed testIntercept4"
    echo "---------------------------------------------------------------"
    echo "Running testGenerateKey"
    testGenerateKey
    echo "Completed testGenerateKey"
    echo "---------------------------------------------------------------"
    echo "Running testIntercepts"
    testIntercepts
    echo "Completed testIntercepts"
    echo "---------------------------------------------------------------"
    echo "Running testInjectGenerateKey"
    testInjectGenerateKey
    echo "Completed testInjectGenerateKey"
    echo "---------------------------------------------------------------"
    echo "Running testKeyListenersFailures"
    testKeyListenersFailures
    echo "Completed testKeyListenersFailures"
    echo "---------------------------------------------------------------"
    echo "Running testInactivity1"
    testInactivity1
    echo "Completed testInactivity1"
    echo "---------------------------------------------------------------"
    echo "Running testInactivity2"
    testInactivity2
    echo "Completed testInactivity2"
    echo "---------------------------------------------------------------"
    echo "Running testInactivity3"
    testInactivity3
    echo "Completed testInactivity3"
    echo "---------------------------------------------------------------"
    echo "Running testInactivity4"
    testInactivity4
    echo "Completed testInactivity4"
    echo "---------------------------------------------------------------"
    echo "Running testGetClients1"
    testGetClients1
    echo "Completed testGetClients1"
    echo "---------------------------------------------------------------"
    echo "Running testGetClients2"
    testGetClients2
    echo "Completed testGetClients2"
    echo "---------------------------------------------------------------"
    echo "Running testGetKeyRepeatsEnabled"
    testGetKeyRepeatsEnabled
    echo "Completed testGetKeyRepeatsEnabled"
    echo "---------------------------------------------------------------"
    echo "Running testInputEventEnable"
    testInputEventEnable
    echo "Completed testInputEventEnable"
    echo "---------------------------------------------------------------"
    echo "Running testInputEventDisable"
    testInputEventDisable
    echo "Completed testInputEventDisable"
    echo "---------------------------------------------------------------"
    echo "Running testIgnoreKeyInputsEnable"
    testIgnoreKeyInputsEnable
    echo "Completed testIgnoreKeyInputsEnable"
    echo "---------------------------------------------------------------"
    echo "Running testIgnoreKeyInputsDisable"
    testIgnoreKeyInputsDisable
    echo "Completed testIgnoreKeyInputsDisable"
    echo "---------------------------------------------------------------"
    echo "Running testKeyRepeatConfigUnsupportedInput"
    testKeyRepeatConfigUnsupportedInput
    echo "Completed testKeyRepeatConfigUnsupportedInput"
    echo "---------------------------------------------------------------"
    echo "Running testKeyRepeatConfigMissingEnabled"
    testKeyRepeatConfigMissingEnabled
    echo "Completed testKeyRepeatConfigMissingEnabled"
    echo "---------------------------------------------------------------"
}

invoke_function() {
    case $function_name in
        testCreateDisplay1)
            testCreateDisplay1
            ;;
        testCreateDisplay2)
            testCreateDisplay2
            ;;
        testCreateDisplay3)
            testCreateDisplay3
            ;;
        testInactivity1)
            testInactivity1
            ;;
        testInactivity2)
            testInactivity2
            ;;
        testInactivity3)
            testInactivity3
            ;;
        testInactivity4)
            testInactivity4
            ;;
        testGetClients1)
            testGetClients1
            ;;
        testGetClients2)
            testGetClients2
            ;;
        testListeners)
            testListeners
            ;;
        testIntercept1)
            testIntercept1
            ;;
        testIntercept2)
            testIntercept2
            ;;
        testIntercept3)
            testIntercept3
            ;;
        testIntercept4)
            testIntercept4
            ;;
        testGenerateKey)
            testGenerateKey
            ;;
        testIntercepts)
            testIntercepts
            ;;
        testInjectGenerateKey)
            testInjectGenerateKey
            ;;
        testKeyListenersFailures)
            testKeyListenersFailures
            ;;
        testGetKeyRepeatsEnabled)
            testGetKeyRepeatsEnabled
            ;;
        testInputEventEnable)
            testInputEventEnable
            ;;
        testInputEventDisable)
            testInputEventDisable
            ;;
        testIgnoreKeyInputsEnable)
            testIgnoreKeyInputsEnable
            ;;
        testIgnoreKeyInputsDisable)
            testIgnoreKeyInputsDisable
            ;;
        testKeyRepeatConfigUnsupportedInput)
            testKeyRepeatConfigUnsupportedInput
            ;;
        testKeyRepeatConfigMissingEnabled)
            testKeyRepeatConfigMissingEnabled
            ;;
        testAll)
            testAll
            ;;

        *)
            echo "Function '$function_name' not found."
            ;;
    esac
}

# Call the invoke_function to start the script
invoke_function

totalCases=$(expr $successCount + $failuresCount)

echo "---------------------------------------------------------------"
echo "Total Test Cases - $totalCases"
echo "Success Test Count - $successCount"
echo "Failed Test Count - $failuresCount"
echo "---------------------------------------------------------------"

if [ $failuresCount -eq 0 ]
then
  exit 0
else
  exit 1
fi
