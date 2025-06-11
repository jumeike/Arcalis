namespace cpp thrift_memcached

service MemcachedService{
    list<i8> getRequest(1:list<i8> key),
    bool setRequest(1:list<i8> key, 2:list<i8> value)
}
