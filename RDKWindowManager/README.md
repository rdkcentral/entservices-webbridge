-----------------
# RDKWindowManager

## Versions
`org.rdk.RDKWindowManager.1`

## Methods:
```
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.RDKWindowManager.1.addKeyIntercept", "params":{"keyCode": 10, "modifiers": ["alt", "shift"], "client": "org.rdk.Netflix"}}' http://127.0.0.1:9998/jsonrpc
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.RDKWindowManager.1.removeKeyIntercept", "params":{"keyCode": 10, "modifiers": ["alt", "shift"], "client": "org.rdk.Netflix"}}' http://127.0.0.1:9998/jsonrpc
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.RDKWindowManager.1.getClients", "params":{}}' http://127.0.0.1:9998/jsonrpc
```

## Responses
```
addKeyIntercept:
{"jsonrpc":"2.0", "id":3, "result": {} }

removeKeyIntercept:
{"jsonrpc":"2.0", "id":3, "result": {} }

getClients:
{"jsonrpc":"2.0", "id":3, "result": { "clients": ["org.rdk.Netflix", "org.rdk.RDKBrowser2"]} }

```

## Events
```
none
```
