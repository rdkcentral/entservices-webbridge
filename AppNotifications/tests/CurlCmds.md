# FbMetrics JSON-RPC cURL Commands

## Controller: Activate / Deactivate org.rdk.FbMetrics

### Activate
```bash
curl  -d '{"jsonrpc": "2.0","id":122,"method":"Controller.1.activate","params":{"callsign":"org.rdk.AppNotifications"}}' http://127.0.0.1:9998/jsonrpc
```

### Deactivate
```bash
curl  -d '{"jsonrpc": "2.0","id":122,"method":"Controller.1.deactivate","params":{"callsign":"org.rdk.AppNotifications"}}' http://127.0.0.1:9998/jsonrpc
```

---

## Action
```bash
curl -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":"100","method":"org.rdk.AppNotifications.1.subscribe","params": {"context": { "requestId":0, "appId":"com.example.app", "connectionId":"someConnectionguid" },"listen": true,"module": "org.rdk.FbSettings.1", "event": "TextToSpeech.Enabled"}}}' http://127.0.0.1:9998/jsonrpc
```