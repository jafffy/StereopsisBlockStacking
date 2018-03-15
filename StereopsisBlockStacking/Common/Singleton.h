#ifndef SINGLETON_H_
#define SINGLETON_H_

template <typename T>
class Singleton
{
protected:
    static T* pSingleton;
public:
    Singleton()
    {
        pSingleton = static_cast<T*>(this);
    }

    static T* get()
    {
        return pSingleton;
    }
};

template <typename T>
T* Singleton<T>::pSingleton = nullptr;

#endif // SINGLETON_H_