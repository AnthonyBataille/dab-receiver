#pragma once

#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"

#include <cstdint>

namespace dab {

void build_diagnostics(IqFileReader& reader, std::uint64_t sample_rate,
                       IqDiagnostics* diagnostics);

} // namespace dab
