#include "coroutine.h"

namespace hco {

std::atomic<size_t> Id::global_increment_id{2};

}