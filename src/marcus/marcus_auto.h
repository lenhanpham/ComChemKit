#ifndef MARCUS_AUTO_H
#define MARCUS_AUTO_H

#include "marcus/marcus_types.h"
#include "job_management/job_scheduler.h"
#include <string>

namespace Marcus {

/** Run all Gaussian jobs sequentially inside the current allocation. */
int run_automatic(MarcusProject project, const std::string& input_file,
                  const JobResources& resources);

} // namespace Marcus

#endif // MARCUS_AUTO_H
