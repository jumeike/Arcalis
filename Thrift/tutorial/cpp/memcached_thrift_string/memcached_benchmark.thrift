namespace cpp thrift_memcached

service MemcachedService{
    string getRequest(1:string key),
    bool setRequest(1:string key, 2:string value)
}
