#include "iq_output.hpp"

#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <iomanip>
#include <initializer_list>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace dab {

PendingCsv::PendingCsv(const std::filesystem::path& destination) {
    if (destination.empty()) {
        throw std::invalid_argument("CSV destination path must not be empty");
    }
    destination_ = std::filesystem::absolute(destination).lexically_normal();
    std::filesystem::create_directories(destination_.parent_path());
    if (std::filesystem::is_directory(destination_)) {
        throw std::runtime_error("CSV destination is a directory: " + destination_.string());
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        temporary_ = destination_;
        temporary_ += ".tmp-" + std::to_string(stamp) + "-" + std::to_string(attempt);
        const HANDLE handle = CreateFileW(temporary_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                          FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
            throw std::runtime_error("Cannot create temporary CSV for: " + destination_.string());
        }
        temporary_.clear();
    }
    if (temporary_.empty()) {
        throw std::runtime_error("Cannot reserve temporary CSV for: " + destination.string());
    }
    backup_ = temporary_;
    backup_ += ".bak";
    stream_.open(temporary_);
    if (!stream_) {
        cleanup();
        throw std::runtime_error("Cannot open temporary CSV for: " + destination.string());
    }
}

PendingCsv::~PendingCsv() {
    cleanup();
}

std::ostream& PendingCsv::stream() {
    return stream_;
}

void PendingCsv::close() {
    stream_.flush();
    if (!stream_) {
        throw std::runtime_error("Error writing diagnostic CSV");
    }
    stream_.close();
    if (!stream_) {
        throw std::runtime_error("Error writing diagnostic CSV");
    }
}

void PendingCsv::publish() {
    if (std::filesystem::exists(destination_)) {
        if (!std::filesystem::is_regular_file(destination_)) {
            throw std::runtime_error("CSV destination is not a file: " + destination_.string());
        }
        // Windows rename cannot replace an existing file. Keep its old bytes
        // until every requested CSV has been published successfully.
        std::filesystem::rename(destination_, backup_);
        backed_up_ = true;
    }
    std::filesystem::rename(temporary_, destination_);
    published_ = true;
}

void PendingCsv::commit() {
    committed_ = true;
    std::error_code ignored;
    if (backed_up_) {
        std::filesystem::remove(backup_, ignored);
    }
}

void PendingCsv::cleanup() noexcept {
    stream_.close();
    std::error_code ignored;
    if (!committed_) {
        if (published_) {
            std::filesystem::remove(destination_, ignored);
        }
        if (backed_up_) {
            std::filesystem::rename(backup_, destination_, ignored);
        }
    }
    if (!temporary_.empty()) {
        std::filesystem::remove(temporary_, ignored);
    }
}

void publish_iq_diagnostics(PendingCsv* power_csv, PendingCsv* spectrum_csv) {

    for (auto csv : {power_csv, spectrum_csv}) {
        if (csv) {
            csv->close();
        }
    }
    for (auto* csv : {power_csv, spectrum_csv}) {
        if (csv) {
            csv->publish();
        }
    }
    for (auto* csv : {power_csv, spectrum_csv}) {
        if (csv) {
            csv->commit();
        }
    }
}

void print_iq_summary(std::ostream& output, const IqSummary& summary) {
    output << "Complex samples: " << summary.num_complex_samples << '\n'
           << "Duration: " << std::fixed << std::setprecision(3) << summary.duration_seconds
           << " s\n";
    if (!output) {
        throw std::runtime_error("Error writing IQ summary");
    }
}

} // namespace dab
