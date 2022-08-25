# Rek DB

A simple key value store database built on `rocksdb` and `cpp-httplib`.

## Requirements
rocksdb
libutil
cpp-httplib
crate

## Usage
```
./rekdb --cfg config.toml
```

## Config
```
[db]
port = 4096
database_location = "/tmp/registrar.db"
```

## Check if database is online

*Endpoint*: ``` / ```

*Example*: ``` 127.0.0.1/ ```

Returns:

```
{"status":200,"data":"success"}
```

## Check if item exists

*Endpoint*: ``` /probe/< key > ```

*Example*: ``` 127.0.0.1/probe/my_key ```

Returns:

```
{ "status": 200, "data": "found" }
```
or
```
{ "status": 200, "data": "not found" }
```

## Submit item

*Endpoint*: ```/submit/< key >/< value > ``` 

*Example*: ``` 127.0.0.1/submit/my_key/my_value ```

Returns:

```
{ "status": 200, "data": "success" }
```


## Fetch item

*Endpoint*: ```/fetch/< key > ``` 

*Example*: ``` 127.0.0.1/fetch/my_key ```

Returns:

```
 < data >
```

or 

```
{ "status": 200, "data": "not found" }
```

if no item is found

## Delete item

*Endpoint*: ```/delete/< key > ```
*Example*: ``` 127.0.0.1/fetch/my_key ```

*Note*: Deleting an item that does not exist returns "okay"
        as does deleting something that did exist

Returns

```
{ "status": 200, "data": "success" }
```
