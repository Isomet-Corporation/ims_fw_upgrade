#pragma once
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

class ProgressBar {
public:
    using Clock = std::chrono::steady_clock;

    ProgressBar(std::function<std::size_t()> getProcessed,
        std::function<std::size_t()> getTotal,
        std::chrono::milliseconds updateInterval = std::chrono::milliseconds(100))
        : m_getProcessed(std::move(getProcessed)),
        m_getTotal(std::move(getTotal)),
        m_updateInterval(updateInterval),
        m_done(false), m_running(false) {
    }

    void start() {
        m_done = false;
        m_running = true;
        m_thread = std::thread(&ProgressBar::run, this);
    }

    void stop() {
        m_done = true;
        m_running = false;
        if (m_thread.joinable())
            m_thread.join();
    }

    bool is_running() { return m_running.load(); }

    ~ProgressBar() {
        stop();
    }

private:
    void run() {
        auto startTime = Clock::now();
        std::size_t lastProcessed = 0;
        auto lastUpdate = startTime;

        while (!m_done) {
            drawProgress(startTime, lastProcessed, lastUpdate);
            std::this_thread::sleep_for(m_updateInterval);
        }

        // Draw final 100% bar
        int termWidth = getTerminalWidth();
        int barWidth = termWidth - 30; // adjust space for stats
        if (barWidth < 10) barWidth = 10;

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) std::cout << "=";
        std::cout << "] 100.00%   Done!\n";
    }

    void drawProgress(Clock::time_point start,
        std::size_t& lastProcessed,
        Clock::time_point& lastUpdate)
    {
        int termWidth = getTerminalWidth();
        int barWidth = termWidth - 50; // leave room for stats
        if (barWidth < 10) barWidth = 10;

        std::size_t processed = m_getProcessed();
        std::size_t total = m_getTotal();
        auto now = Clock::now();

        double progress = total > 0 ? static_cast<double>(processed) / total : 0.0;
        int pos = static_cast<int>(barWidth * progress);

        double deltaBytes = static_cast<double>(processed - lastProcessed);
        double deltaTime = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastUpdate).count();
        double speed = (deltaTime > 0) ? deltaBytes / deltaTime : 0.0;  // bytes/sec

        double remaining = (speed > 0 && total > 0) ? (total - processed) / speed : 0.0;
        int etaSec = static_cast<int>(remaining);
        int etaMin = etaSec / 60;
        etaSec %= 60;

        // Human-readable speed
        const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s" };
        double displaySpeed = speed;
        int unitIndex = 0;
        while (displaySpeed > 1024.0 && unitIndex < 3) {
            displaySpeed /= 1024.0;
            ++unitIndex;
        }

        std::ostringstream oss;
        oss << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) oss << "=";
            else if (i == pos) oss << ">";
            else oss << " ";
        }

        oss << "] ";
        oss << std::setw(6) << std::fixed << std::setprecision(2)
            << (progress * 100.0) << "%";
        oss << "  " << std::setw(6) << std::fixed << std::setprecision(2)
            << displaySpeed << " " << units[unitIndex];
        if (progress < 1.0)
            oss << "  ETA: " << std::setw(2) << std::setfill('0') << etaMin
            << ":" << std::setw(2) << etaSec << "   ";
        else
            oss << "  ETA: 00:00   ";

        std::cout << oss.str();
        std::cout.flush();

        lastProcessed = processed;
        lastUpdate = now;
    }

    int getTerminalWidth() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
#else
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            return w.ws_col;
        }
#endif
        return 80; // fallback width
    }

private:
    std::function<std::size_t()> m_getProcessed;
    std::function<std::size_t()> m_getTotal;
    std::chrono::milliseconds m_updateInterval;
    std::atomic<bool> m_done;
    std::atomic<bool> m_running;
    std::thread m_thread;
};
