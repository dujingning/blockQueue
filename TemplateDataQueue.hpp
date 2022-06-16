#ifndef CQUEUE_H
#define CQUEUE_H

#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <cstdint>
#include <iostream>

/*	
 * 1. do not allow to call function by function when handle same lock
 * 2. call disableQueue() before destory for wakeup pop();
 * 3. see 'define' under the declaration; see 'usage' under the define
 * 4. typename of T must support assignment by '='
 * 5. powered by C++ 11
 * 
 * auth    : dujingning
 * date    : 2021.07.24
 * license : MIT
*/
template <typename  T>
class Cqueue
{
public:
    Cqueue();
    ~Cqueue();

public:
    void (*clear)(T data);
    /* queue method */
    bool empty();
    T front();
    size_t size();
    bool push(const T data);
    bool pop(T &data);

    /* setup */
    void disableQueue();
    void disableMaxLen();

    void setMaxLen(int len);
    unsigned int getMaxLen();
    void destroyAllData();

    Cqueue (const Cqueue&) = delete;
    Cqueue operator = (const Cqueue&) = delete;

private:
    std::queue<T> m_Std_Queue;
    std::mutex m_Std_Queue_mutex;
    std::condition_variable m_std_Cond;

    volatile std::atomic<bool> m_DisableMaxLen;
    volatile std::atomic<unsigned int>  m_maxLen;
    volatile std::atomic<bool> m_enable;
};


template <typename  T>
Cqueue <T>::Cqueue()
{
    m_enable = true;
    clear = nullptr;
    m_DisableMaxLen = true;
    m_maxLen = 200;
}

template <typename  T>
Cqueue <T>::~Cqueue()
{
    disableQueue();
    destroyAllData();
}

template <typename  T>
void Cqueue <T>::disableQueue()
{
    m_enable = false;
    m_std_Cond.notify_all();
}

template <typename  T>
void Cqueue <T>::disableMaxLen()
{
    m_DisableMaxLen = false;
}

template <typename  T>
void Cqueue <T>::setMaxLen(int len)
{
    m_maxLen = len;
}

template <typename  T>
unsigned int Cqueue <T>::getMaxLen()
{
    return m_maxLen;
}

template <typename  T>
bool Cqueue <T>::empty()
{
    std::lock_guard<std::mutex> lock(m_Std_Queue_mutex);
    return m_Std_Queue.empty();
}

template <typename  T>
T Cqueue <T>::front()
{
    std::lock_guard<std::mutex> lock(m_Std_Queue_mutex);
    return m_Std_Queue.front();
}

template <typename  T>
size_t Cqueue <T>::size()
{
    std::lock_guard<std::mutex> lock(m_Std_Queue_mutex);
    return m_Std_Queue.size();
}

template <typename  T>
bool Cqueue <T>::push(const T data)
{
    if (!m_enable ){
        return false;
    } else {
		std::lock_guard<std::mutex> lock(m_Std_Queue_mutex);
		if(m_DisableMaxLen && m_Std_Queue.size() > m_maxLen) {
			std::cout << "[Cqueue][push] out of Max Queue length,drop data!" << std::endl;
			return false;
		}
		m_Std_Queue.push(data);
	}
    m_std_Cond.notify_one();
    return true;
}

template <typename  T>
bool Cqueue <T>::pop(T &data)
{
    if (!m_enable){
        return false;
    }

    std::unique_lock<std::mutex> lock(m_Std_Queue_mutex);
    if (m_enable && m_Std_Queue.empty()) { /* 防止虚假唤醒 */
        m_std_Cond.wait(lock, [this] { return !m_enable || !m_Std_Queue.empty(); });
    }

    if (m_enable && !m_Std_Queue.empty()){
        data = m_Std_Queue.front();
        m_Std_Queue.pop();
        return true;
    }
    return false;
}

template <typename  T>
void Cqueue <T>::destroyAllData()
{
    if (clear == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Std_Queue_mutex);
	/* drop data */
    while(!m_Std_Queue.empty()){
        T data =  m_Std_Queue.front();
        m_Std_Queue.pop();
        clear(data);
    }
}

#endif /* CQUEUE_H  */


#ifdef DEBUG

#define sleep(for_seconds)   std::this_thread::sleep_for(std::chrono::seconds(for_seconds))
#define usleep(for_milliseconds)        std::this_thread::sleep_for(std::chrono::milliseconds(for_milliseconds))

Cqueue<int> mQueue;
std::atomic<uint32_t> i(0);
void push(){
	while(true) {
		if (mQueue.push(i)) {
			++i;
			if (i%100000 == 0)
				std::cout << "push " << i << std::endl;
		} else {
			usleep(0);
		}
	}
}

void pop(){
	int i;
	while(true) {
		if (mQueue.pop(i)) {
			if(i%100000 == 0)
				std::cout << "pop " << i << std::endl;
		}
	}
}

/* compile: g++ -std=c++11 TemplateDataQueue.cpp -DDEBUG -pthread */
int main(){
	mQueue.disableMaxLen();
	
	std::thread tPush(push);
	std::thread tPop(pop);
	
	tPush.join();
	tPop.join();
}
#endif