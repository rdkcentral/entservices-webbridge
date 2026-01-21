-----------------
# StorageManager

## Versions
`org.rdk.StorageManager.1`

## Methods:
```
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method": "org.rdk.StorageManager.clear", "params": {"appId": "com.sky.testapp"}}' http://127.0.0.1:9998/jsonrpc
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method": "org.rdk.StorageManager.clearAll", "params": {"exemptionAppIds": "com.sky.testapp"}}' http://127.0.0.1:9998/jsonrpc
```

## Responses
```
clear:
{"jsonrpc":"2.0","id":3,"result":""}

clearAll:
{"jsonrpc":"2.0","id":3,"result":""}

```

## Events
```
none
```
