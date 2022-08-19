# Rek DB

A simple key value store recording database


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

## Check if item exists

*Endpoint*: /probe

```
{ "key": "entry_key" }
```

Returns:

```
{ "status": 200, "data": "found" }
```
or 
```
{ "status": 200, "data": "not found" }
```

## Submit item

*Endpoint*: /submit

*Note* : Value must resolve to string data.
         any data that is stringed json should be
         B64 or hex encoded to remove parsable json data

```
{ "key": "entry_key", "value": "string data" }
```

Returns:

```
{ "status": 200, "data": "success" }
```


## Fetch item

*Endpoint*: /fetch

```
{ "key": "entry_key" }
```

Returns:

```
{ "status": 200, "data": "entry data" }
```

or 

```
{ "status": 200, "data": "" }
```

if no item is found

## Delete item

*Endpoint*: /delete

*Note*: Deleting an item that does not exist returns "okay"
        as does deleting something that did exist

```
{ "key": "entry_key" }
```

returns

```
{ "status": 200, "data": "okay" }
```
