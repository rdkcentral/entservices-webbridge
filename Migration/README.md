-----------------
# Migration

## Callsign
`org.rdk.Migration`

## Methods:
```
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc": "2.0","id":3,"method": "Controller.1.activate","params": {"callsign": "org.rdk.Migration"}}' http://127.0.0.1:9998/jsonrpc
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.Migration.getBootTypeInfo"}' http://127.0.0.1:9998/jsonrpc
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.Migration.getMigrationStatus"}' http://127.0.0.1:9998/jsonrpc
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":3, "method":"org.rdk.Migration.setMigrationStatus","params":{"status": NOT_STARTED}}' http://127.0.0.1:9998/jsonrpc
```

## Responses
```
getBootTypeInfo:
{"jsonrpc":"2.0","id":3,"result":{"bootType":"BOOT_NORMAL"}}

getMigrationStatus:
{"jsonrpc":"2.0","id":3,"result":{"migrationStatus":"NOT_NEEDED"}}

setMigrationStatus:
{"jsonrpc":"2.0","id":3,"result":{"success":true}}
```

## Events
```
none
```
