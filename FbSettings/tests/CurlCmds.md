# FbMetrics JSON-RPC cURL Commands

## Controller: Activate / Deactivate org.rdk.FbMetrics

### Activate
```bash
curl  -d '{"jsonrpc": "2.0","id":122,"method":"Controller.1.activate","params":{"callsign":"org.rdk.FbSettings"}}' http://127.0.0.1:9998/jsonrpc
```

### Deactivate
```bash
curl  -d '{"jsonrpc": "2.0","id":122,"method":"Controller.1.deactivate","params":{"callsign":"org.rdk.FbSettings"}}' http://127.0.0.1:9998/jsonrpc
```

---

## Action
```bash
curl -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":"100","method":"org.rdk.FbSettings.1.handleAppEventNotifier","params": {"listen": true,"event": "TextToSpeech.Enabled"}}}' http://127.0.0.1:9998/jsonrpc
```


## org.rdk.FbSettings.1: org.rdk.System callsign-derived aliases

Below are example requests for each of the 13 methods exposed by org.rdk.FbSettings.1. 
- For getters/response-only methods, no params are included.
- For methods that require input, an example params object is provided.
- For subscribe methods, the listen parameter is demonstrated.

1) getDeviceMake
- Tests retrieving the device make (maps to org.rdk.System.getDeviceInfo)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.FbSettings.1.getDeviceMake"}' \
  http://127.0.0.1:9998/jsonrpc
```

2) getDeviceName
- Tests retrieving the device friendly name (maps to org.rdk.System.getFriendlyName)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"org.rdk.FbSettings.1.getDeviceName"}' \
  http://127.0.0.1:9998/jsonrpc
```

3) setDeviceName
- Tests setting the device friendly name (maps to org.rdk.System.setFriendlyName)
- Example provides a sample name
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":3,"method":"org.rdk.FbSettings.1.setDeviceName","params":{"name":"Living Room"}}' \
  http://127.0.0.1:9998/jsonrpc
```

4) getDeviceSku
- Tests retrieving the device SKU (maps to org.rdk.System.getSystemVersions)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":4,"method":"org.rdk.FbSettings.1.getDeviceSku"}' \
  http://127.0.0.1:9998/jsonrpc
```

5) getCountryCode
- Tests retrieving the country code (maps to org.rdk.System.getTerritory -> Firebolt country code)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":5,"method":"org.rdk.FbSettings.1.getCountryCode"}' \
  http://127.0.0.1:9998/jsonrpc
```

6) setCountryCode
- Tests setting the country code (maps to org.rdk.System.setTerritory)
- Example provides a sample country code (US)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":6,"method":"org.rdk.FbSettings.1.setCountryCode","params":{"countryCode":"US"}}' \
  http://127.0.0.1:9998/jsonrpc
```

7) getTimeZone
- Tests retrieving the current timezone (maps to org.rdk.System.getTimeZoneDST)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":8,"method":"org.rdk.FbSettings.1.getTimeZone"}' \
  http://127.0.0.1:9998/jsonrpc
```

8) setTimeZone
- Tests setting the current timezone (maps to org.rdk.System.setTimeZoneDST)
- Example provides an illustrative timezone
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":9,"method":"org.rdk.FbSettings.1.setTimeZone","params":{"timeZone":"America/New_York"}}' \
  http://127.0.0.1:9998/jsonrpc
```

9) getSecondScreenFriendlyName
- Tests retrieving the second screen friendly name (alias of getDeviceName)
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":11,"method":"org.rdk.FbSettings.1.getSecondScreenFriendlyName"}' \
  http://127.0.0.1:9998/jsonrpc
```
