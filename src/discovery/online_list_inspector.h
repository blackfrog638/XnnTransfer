#pragma once
namespace discovery {
class OnlineListInspector {
  public:
    OnlineListInspector() = default;
    ~OnlineListInspector() = default;

    OnlineListInspector(const OnlineListInspector&) = delete;
    OnlineListInspector& operator=(const OnlineListInspector&) = delete;
};
} // namespace discovery