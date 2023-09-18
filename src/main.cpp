#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <cstdlib> // for rand
#include <memory> // for shared_ptr

using namespace std;
using namespace std::chrono_literals;

const auto getRandomWaitTime()
{
    static int MAX_WAIT_TIME = 5000; // 5s
    return 100 + (std::rand() / ((RAND_MAX + 1u) / MAX_WAIT_TIME));
}

class Request
{
public:
    void doRequest()
    {
        std::cout << "Do Request at thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(1s);
        std::cout << "Done" << std:: endl;
    }
};

class RequestController
{
public:
    // возвращает nullptr если нужно завершить процесс, либо указатель на память,
    // которую в дальнейшем требуется удалить
    Request* GetRequest() throw()
    {
        // Вызовы GetRequest() и ProcessRequest() могут работать долго.
        const auto waitTime = std::chrono::milliseconds(getRandomWaitTime() / 2); // пусть выдает задачи быстрее, чем решает их в 2 раза
        std::this_thread::sleep_for(waitTime);
        std::cout << "wait ms: " << waitTime.count() << std::endl;

        if(!Started())
        {
            return {};
        }

        try
        {
            requestCount_++;
            return new Request();
        }
        catch(...)
        {
            // do smth
            return {};
        }
    }

    // обрабатывает запрос, но память не удаляет
    void ProcessRequest(Request* request) throw()
    {
        // Вызовы GetRequest() и ProcessRequest() могут работать долго.
        const auto waitTime = std::chrono::milliseconds(getRandomWaitTime());
        std::this_thread::sleep_for(waitTime);
        std::cout << "wait ms: " << waitTime.count() << std::endl;

        if(request)
        {
            try
            {
                request->doRequest();
            }
            catch(...)
            {
                // do smth
            }
        }
    }

    void PushRequest(Request* request)
    {
        if(!request)
        {
            return;
        }

        const std::lock_guard<std::mutex> lock(requestMutex_);
        requestQueue_.push(request);
        std::cout << "New request count: " << requestQueue_.size() << std::endl;
    }

    bool Started()
    {
        const std::lock_guard<std::mutex> lock(stopMutex_);
        return isStarted_ && requestCount_ < stopAfterRequest_; // считаем, что нужно остановить или по команде ProcessStop или при достижении stopAfterRequest_
    }

    void ProcessStop()
    {
        const std::lock_guard<std::mutex> lock(stopMutex_);
        isStarted_ = false;
    }

    Request* TakeFrontRequest()
    {
        const std::lock_guard<std::mutex> lock(requestMutex_);
        if(requestQueue_.empty())
        {
            return {};
        }

        const auto request = requestQueue_.front();
        requestQueue_.pop();
        return request;
    }
private:
    std::mutex requestMutex_;
    std::mutex stopMutex_;
    std::queue<Request*> requestQueue_;
    bool isStarted_ = true;

    // программный признак "нужно завершить процесс"
    int requestCount_ = 0;
    const int stopAfterRequest_ = 1000;
};

const int NumberOfThreads = 2;

void threadWorkerFunction(std::shared_ptr<RequestController> controller)
{
    //2)	Завершиться, как только основной поток ему это скомандует.
    while(controller->Started())
    {
        if(const auto request = controller->TakeFrontRequest())
        {
            // 1)	Обрабатывать поступающие через очередь запросы с помощью ProcessRequest.
            request->doRequest();
            // ... на память, которую в дальнейшем требуется удалить
            delete request;
        }
    }
}

// отдельный поток пользовательского ввода. Ждет 'Q' для "нужно завершить процесс"
void threadUserInput(std::shared_ptr<RequestController> controller)
{
    char ch = 0;
    while(controller->Started() && ch != 'Q')
    {
        std::cin >> ch;
    }

    // ... GetRequest возвращает nullptr если нужно завершить процесс
    controller->ProcessStop();
}

int main()
{
    std::srand(std::time(nullptr)); // инициализация random для различного времени ожидания.
    const auto controller = std::make_shared<RequestController>();

    // Запустить поток ввода пользователя. Для возможности останова controller
    auto inputThread = std::thread(threadUserInput, controller);

    //1)	Запустить несколько рабочих потоков (NumberOfThreads).
    std::vector<std::thread> threads(NumberOfThreads);
    for(auto &thread : threads)
    {
        thread = std::thread(threadWorkerFunction, controller);
    }

    //2)	Класть в одну очередь заданий задачи до тех пор, пока GetRequest() не вернёт nullptr.
    while(const auto& request = controller->GetRequest())
    {
        controller->PushRequest(request);
    }

    //3)	Корректно остановить рабочие потоки.
    // Они должны доделать текущий ProcessRequest, если он имеется, и остановиться.
    // Если имеются необработанные задания, не обращать на них внимания.
    controller->ProcessStop();
    for(auto &thread : threads)
    {
        thread.join();
    }

    inputThread.join(); // Ждать завершения потока ввода пользователя

    //4)	Завершить программу.
    return 0;
}
