#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <type_traits>
#include <vector>

// Forward declarations: Khai báo trước các lớp để compiler nhận biết sự tồn tại của chúng
class Component;
class Entity;
class Manager;

// Định danh kiểu cho ID của Component và Group
using ComponentID = std::size_t;
using Group = std::size_t; // Phân nhóm thực thể (ví dụ: gạch bản đồ, người chơi, va chạm...)

// Sinh ra một Component Type ID duy nhất tăng dần cho mỗi loại Component mới
[[nodiscard]] inline ComponentID getNewComponentTypeID() noexcept {
  static ComponentID lastID = 0u;
  return lastID++;
}

// Lấy ra Type ID cố định cho kiểu Component lớp T (chỉ sinh ra 1 lần duy nhất cho mỗi lớp con của Component)
template <typename T>
[[nodiscard]] inline ComponentID getComponentTypeID() noexcept {
  // Ràng buộc kiểm tra tại thời điểm compile: T bắt buộc phải kế thừa từ lớp Component
  static_assert(std::is_base_of_v<Component, T>,
                "Kieu du lieu T phai ke thua tu lop Component!");
  static ComponentID typeID = getNewComponentTypeID();
  return typeID;
}

// Định nghĩa số lượng tối đa Component và Group phân loại
constexpr std::size_t maxComponents = 32;
constexpr std::size_t maxGroups = 32;

// Định nghĩa các cấu trúc lưu trữ dựa trên bitset và array tĩnh để tối ưu hóa hiệu năng truy xuất
using ComponentBitSet = std::bitset<maxComponents>; // Tập bit lưu trữ trạng thái có/không có component
using GroupBitset = std::bitset<maxGroups>;         // Tập bit lưu trữ trạng thái thuộc về nhóm group nào
using ComponentArray = std::array<Component *, maxComponents>; // Mảng tĩnh lưu trữ con trỏ tới các Component của thực thể

// Lớp cơ sở Component: Mọi thành phần chức năng (vị trí, sprite vẽ, va chạm...) đều kế thừa từ đây
class Component {
public:
  Entity *entity{nullptr}; // Con trỏ trỏ ngược về thực thể sở hữu component này

  virtual void init() {}   // Khởi tạo component (gọi ngay sau khi được add vào entity)
  virtual void update() {} // Cập nhật logic game mỗi frame
  virtual void draw() {}   // Vẽ đồ họa mỗi frame
  virtual ~Component() = default; // Hủy ảo đảm bảo giải phóng lớp con an toàn
};

// Lớp Entity đại diện cho một đối tượng trong trò chơi (như Người chơi, Quái vật, Cây cối, Viên đạn...)
// Entity chỉ đóng vai trò là một container rỗng chứa các Component và quản lý vòng đời của chúng.
class Entity {
private:
  Manager &manager; // Tham chiếu đến lớp Manager quản lý thực thể này
  bool active = true; // Trạng thái hoạt động của thực thể (nếu false sẽ bị xóa ở frame tiếp theo)
  std::vector<std::unique_ptr<Component>> components; // Danh sách các component độc quyền sở hữu

  ComponentArray componentArray{}; // Mảng tĩnh ánh xạ nhanh ID Component sang con trỏ Component
  ComponentBitSet componentBitset; // Tập bit ghi nhận các Component đang sở hữu
  GroupBitset groupBitset;         // Tập bit ghi nhận các Group mà thực thể thuộc về

public:
  // Khởi tạo Entity liên kết với Manager quản lý
  Entity(Manager &mManager) : manager(mManager) {}

  // Cập nhật logic tất cả các component thuộc thực thể này
  void update() {
    for (auto &c : components)
      c->update();
  }

  // Vẽ đồ họa của tất cả các component thuộc thực thể này
  void draw() {
    for (auto &c : components)
      c->draw();
  }

  // Kiểm tra xem thực thể còn hoạt động không
  [[nodiscard]] bool isActive() const noexcept { return active; }

  // Đánh dấu thực thể đã chết hoặc bị hủy (để Manager dọn dẹp)
  void destroy() noexcept { active = false; }

  // Kiểm tra xem thực thể có thuộc về một nhóm (group) cụ thể không
  [[nodiscard]] bool hasGroup(Group mGroup) const noexcept {
    return groupBitset[mGroup];
  }

  // Thêm thực thể vào một nhóm cụ thể (định nghĩa ở cuối file)
  void addGroup(Group mGroup);

  // Xóa thực thể khỏi một nhóm cụ thể
  void delGroup(Group mGroup) noexcept { groupBitset[mGroup] = false; }

  // Kiểm tra xem thực thể có sở hữu component kiểu T hay không
  template <typename T> [[nodiscard]] bool hasComponent() const noexcept {
    return componentBitset[getComponentTypeID<T>()];
  }

  // Thêm mới một Component kiểu T vào thực thể
  // Trả về tham chiếu đến Component vừa tạo
  template <typename T, typename... TArgs> T &addComponent(TArgs &&...mArgs) {
    // Tạo mới component và chuyển tiếp các tham số khởi tạo (Perfect Forwarding)
    T *c(new T(std::forward<TArgs>(mArgs)...));
    c->entity = this; // Gán con trỏ entity sở hữu
    std::unique_ptr<Component> uPtr{c};
    components.emplace_back(std::move(uPtr)); // Đưa vào quản lý độc quyền

    // Cập nhật mảng ánh xạ và tập bit đánh dấu sở hữu
    componentArray[getComponentTypeID<T>()] = c;
    componentBitset[getComponentTypeID<T>()] = true;

    // Gọi hàm khởi tạo riêng của Component đó
    c->init();
    return *c;
  }

  // Lấy ra tham chiếu đến Component kiểu T đang sở hữu (sử dụng static_cast để ép kiểu nhanh từ con trỏ cơ sở)
  template <typename T> [[nodiscard]] T &getComponent() const {
    auto ptr(componentArray[getComponentTypeID<T>()]);
    return *static_cast<T *>(ptr);
  }
};

// Lớp Manager quản lý danh sách toàn bộ thực thể trong màn chơi, phân nhóm chúng
// và thực hiện dọn dẹp các thực thể đã bị hủy.
class Manager {
private:
  std::vector<std::unique_ptr<Entity>> entities; // Danh sách tất cả các thực thể đang tồn tại
  std::array<std::vector<Entity *>, maxGroups> groupedEntities; // Mảng danh sách thực thể phân loại theo nhóm

public:
  // Cập nhật logic của tất cả thực thể
  void update() {
    for (auto &e : entities)
      e->update();
  }

  // Vẽ đồ họa của tất cả thực thể
  void draw() {
    for (auto &e : entities)
      e->draw();
  }

  // Hàm dọn dẹp (Refresh): Loại bỏ các thực thể đã bị đánh dấu hủy (active == false)
  // ra khỏi các nhóm và danh sách thực thể chính để giải phóng bộ nhớ
  void refresh() {
    // 1. Dọn dẹp danh sách phân nhóm
    for (auto i = 0u; i < maxGroups; i++) {
      auto &v = groupedEntities[i];
      std::erase_if(v, [i](Entity *mEntity) {
        return !mEntity->isActive() || !mEntity->hasGroup(i);
      });
    }

    // 2. Dọn dẹp và hủy vùng nhớ của Entity trong danh sách chính
    std::erase_if(entities, [](const std::unique_ptr<Entity> &mEntity) {
      return !mEntity->isActive();
    });
  }

  // Thêm một thực thể vào một danh sách nhóm cụ thể
  void AddToGroup(Entity *mEntity, Group mGroup) {
    groupedEntities[mGroup].emplace_back(mEntity);
  }

  // Lấy ra danh sách các thực thể thuộc một nhóm cụ thể (để vẽ hoặc xử lý va chạm hàng loạt)
  [[nodiscard]] std::vector<Entity *> &getGroup(Group mGroup) {
    return groupedEntities[mGroup];
  }

  // Tạo mới một Entity trống, đưa vào quản lý và trả về tham chiếu của nó
  Entity &addEntity() {
    Entity *e = new Entity(*this);
    std::unique_ptr<Entity> uPtr{e};
    entities.emplace_back(std::move(uPtr));
    return *e;
  }
};

// Định nghĩa hàm thành viên addGroup của Entity sau khi Manager đã được khai báo hoàn chỉnh
inline void Entity::addGroup(Group mGroup) {
  groupBitset[mGroup] = true;
  manager.AddToGroup(this, mGroup); // Báo cho Manager lưu trữ thực thể này vào nhóm mGroup
}
