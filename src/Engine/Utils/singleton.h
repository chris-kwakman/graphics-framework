#ifndef SINGLETON_H
#define SINGLETON_H 
template<typename T> T & Singleton() {static T instance; return instance;}
#endif