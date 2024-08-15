
// A Scalar that asserts for uninitialized access.
template <typename T>
class SafeScalar {
 public:
  SafeScalar() : initialized_(false) {}
  SafeScalar(const SafeScalar& other) { *this = other; }
  SafeScalar& operator=(const SafeScalar& other) {
    val_ = T(other);
    initialized_ = true;
    return *this;
  }

  SafeScalar(T val) : val_(val), initialized_(true) {}
  SafeScalar& operator=(T val) {
    val_ = val;
    initialized_ = true;
  }

  operator T() const {
    VERIFY(initialized_ && "Uninitialized access.");
    return val_;
  }

 private:
  T val_;
  bool initialized_;
};

namespace Eigen {
namespace internal {
template <typename T>
struct random_impl<SafeScalar<T>> {
  using SafeT = SafeScalar<T>;
  using Impl = random_impl<T>;
  static EIGEN_DEVICE_FUNC inline SafeT run(const SafeT& x, const SafeT& y) {
    T result = Impl::run(x, y);
    return SafeT(result);
  }
  static EIGEN_DEVICE_FUNC inline SafeT run() {
    T result = Impl::run();
    return SafeT(result);
  }
};
}  // namespace internal
}  // namespace Eigen
