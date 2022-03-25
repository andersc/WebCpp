#include <signal.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <chrono>
#include "common_webcpp.h"
#include "HttpClient.h"
#include "Request.h"
#include "StringUtil.h"
#include "FileSystem.h"
#include "Url.h"
#include "Data.h"
#include "DebugPrint.h"
#include "ThreadWorker.h"
#include "example_common.h"

#define DEFAULT_CLIENT_COUNT 2
#define DEFAULT_RESOURCE "http://httpbin.org/get"
#define DEFAULT_DELAY 100
#define TEST_COUNT 100

size_t clientCount = DEFAULT_CLIENT_COUNT;
std::string resource = DEFAULT_RESOURCE;
long delay = DEFAULT_DELAY;
bool g_running = true;
static int g_id = 0;


void handle_sigint(int)
{
    g_running = false;
}

void *ThreadRoutine(bool &running)
{
    int id = g_id++;
    int cnt = 0;
    std::cout << "starting thread " << id << std::endl;

    try
    {
        WebCpp::HttpClient httpCient;
        WebCpp::HttpConfig config;
        config.SetTempFile(true);
        config.SetMaxBodyFileSize(10_Mb);

        if(httpCient.Init(config))
        {
            httpCient.SetResponseCallback([&httpCient,id](const WebCpp::Response &response) -> bool
            {
                std::cout
                        << id
                        << ": response code: "
                        << response.GetResponseCode()
                        << ", size:  " << response.GetHeader().GetRequestSize()
                        << std::endl;

                //StringUtil::PrintHex(response.GetBody());
                return true;
            });

            while(running && g_running)
            {
                if(httpCient.Open(WebCpp::Http::Method::GET, resource) == true)
                {
                    usleep(delay * 1000);
                    cnt ++;
                    if(cnt >= TEST_COUNT)
                    {
                        running = false;
                    }
                }
                else
                {
                    std::cout << "thread " << id << " failed to send request: " << httpCient.GetLastError() << std::endl;
                    running = false;
                }
            }

            httpCient.Close(false);
        }
    }
    catch(...)
    {
        running = false;
    }

    std::cout << "finishing thread " << id << std::endl;

    return nullptr;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);

    auto cmdline = CommandLine::Parse(argc, argv);

    if(cmdline.Exists("-h"))
    {
        cmdline.PrintUsage(false, false);
        exit(0);
    }

    int v;
    if(StringUtil::String2int(cmdline.Get("-c"), v))
    {
        clientCount = v;
    }

    if(cmdline.Exists("-r"))
    {
        resource = cmdline.Get("-r");
    }

    if(StringUtil::String2int(cmdline.Get("-d"), v))
    {
        delay = v;
    }

    std::vector<WebCpp::ThreadWorker> workers;
    workers.resize(clientCount);
    for(size_t i = 0;i < clientCount;i ++)
    {
        auto &worker = workers.at(i);
        worker.SetFunction(ThreadRoutine);
        worker.Start();
    }

    for(size_t i = 0;i < clientCount;i ++)
    {
        auto &worker = workers.at(i);
        worker.Wait();
    }

    return 0;
}
