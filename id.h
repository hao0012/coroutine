#ifndef ID_H_
#define ID_H_

#include <cstddef>
#include <atomic>

namespace hco {
class Id {
 public:
  static Id get_main_co_id() { 
    static Id main_id(MAIN_CO_ID); 
    return main_id;
  }
  static Id get_invalid_id() { 
    static Id invalid_id(INVALID_ID);
    return invalid_id;
  }
  
  Id(): id_(global_increment_id.fetch_add(1)) {}
  explicit constexpr Id(std::size_t id) : id_(id) {}

  std::size_t get_id() const { return id_; }
  static std::size_t get_global_id() { return global_increment_id.load(); }
  bool is_valid() const { return id_ != INVALID_ID; }
  bool operator==(const Id& other) const { return id_ == other.id_; }
  bool operator!=(const Id& other) const { return id_ != other.id_; }

 private:
  static constexpr std::size_t INVALID_ID = 0;
  static constexpr std::size_t MAIN_CO_ID = 1;
  static std::atomic<size_t> global_increment_id;
  std::size_t id_{0};
};

}

template<>
struct std::hash<hco::Id> {
  std::size_t operator()(const hco::Id& id) const noexcept {
    return std::hash<std::size_t>{}(id.get_id());  // 需要将get_id()改为const
  }
};

#endif