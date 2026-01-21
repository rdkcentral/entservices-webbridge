


curl -d '{
  "jsonrpc": "2.0",
  "id": 122,
  "method": "Controller.1.activate",
  "params": { "callsign": "org.rdk.AppGateway" }
}' http://127.0.0.1:9998/jsonrpc


curl -d '{
  "jsonrpc": "2.0",
  "id": 122,
  "method": "Controller.1.deactivate",
  "params": { "callsign": "org.rdk.AppGateway" }
}' http://127.0.0.1:9998/jsonrpc


curl -d '{
  "jsonrpc": "2.0",
  "id": 122,
  "method": "org.rdk.AppGateway.1.resolve",
  "params": { "context" : {"requestId" : 1, "connectionId" : 3, "appId" : "Peacock"},
    "method" : "Accessibility.audioDescriptionSettings"
      }
}' http://127.0.0.1:9998/jsonrpc

curl -d '{
  "jsonrpc": "2.0",
  "id": 122,
  "method": "org.rdk.AppGateway.1.resolve",
  "params": { "context" : {"requestId" : 1, "connectionId" : 3, "appId" : "Peacock"},
    "method" : "Advertising.advertisingId",
    "params" : {}
      }
}' http://127.0.0.1:9998/jsonrpc

curl -d '{
  "jsonrpc": "2.0",
  "id": 122,
  "method": "org.rdk.AppGateway.1.respond"
}' http://127.0.0.1:9998/jsonrpc

