#include <iostream>
#include <queue>
#include <thread>
#include <mutex>

using namespace std;
using namespace std::chrono_literals;

std::mutex requestMutex;
std::mutex stopMutex;
constexpr int NumberOfThreads = 20;

class Request
{
public:
    void doRequest()
    {
        std::cout << "Do Request at thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(2s);
        std::cout << "Done" << std:: endl;
        std::this_thread::sleep_for(1s);
    }
};

std::queue<Request*> requestQueue;
bool isStopRequested = false;

auto shouldWork = []()
{
    const std::scoped_lock lock{requestMutex, stopMutex};
    return !isStopRequested && requestQueue.size() < 5000000;
};

// возвращает nullptr если нужно завершить процесс, либо указатель на память,
// которую в дальнейшем требуется удалить
Request* GetRequest() throw()
{
    if(!shouldWork())
    {
        return nullptr;
    }

    return new Request();
}

// обрабатывает запрос, но память не удаляет
void ProcessRequest(Request* request) throw()
{
    if(!request)
    {
        return;
    }

    const std::lock_guard<std::mutex> lock(requestMutex);
    requestQueue.push(request);
}

void threadWorkerFunction()
{
    while(shouldWork())
    {
        if(requestQueue.empty())
        {
            continue;
        }

        Request* request = nullptr;
        {
            const std::lock_guard<std::mutex> lock(requestMutex);
            request = requestQueue.front();
        }

        if(request)
        {
            request->doRequest();
            {
                const std::lock_guard<std::mutex> lock(requestMutex);
                requestQueue.pop();
                std::cout << " size: "<< requestQueue.size() << std::endl;
            }
        }
    }
}

int main()
{
    std::vector<std::thread> threads(NumberOfThreads);
    for(auto &thread : threads)
    {
        thread = std::thread(threadWorkerFunction);
    }

    while(const auto& request = GetRequest())
    {
        ProcessRequest(request);
    }

    {
        const std::lock_guard<std::mutex> lock(stopMutex);
        isStopRequested = true;
    }

    for(auto &thread : threads)
    {
        thread.join();
    }

    return 0;
}
