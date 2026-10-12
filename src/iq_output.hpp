#pragma once

#include "offline_iq_runner.hpp"

#include <filesystem>
#include <fstream>
#include <iosfwd>

namespace dab {

// Stage beside the destination so publication stays on the same volume.
class PendingCsv {
  public:
    PendingCsv(const std::filesystem::path& destination);

    ~PendingCsv();

    std::ostream& stream();

    void close();

    void publish();

    void commit();

  private:
    void cleanup() noexcept;

    std::filesystem::path destination_, temporary_, backup_;
    std::ofstream stream_;
    bool backed_up_ = false;
    bool published_ = false;
    bool committed_ = false;
};

// Close all outputs before publication; roll back unless all publications succeed.
void publish_iq_diagnostics(PendingCsv* power_csv, PendingCsv* spectrum_csv);
void print_iq_summary(std::ostream& output, const IqSummary& summary);

} // namespace dab
