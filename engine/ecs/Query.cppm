export module ECS.Query;

import ECS.Component;
import Core.Types;
import std;

export template<typename T> struct With {};
export template<typename T> struct Without {};
export template<typename T> struct Optional {};

template<typename T> struct is_filter : std::false_type {};
template<typename T> struct is_filter<With<T>> : std::true_type {};
template<typename T> struct is_filter<Without<T>> : std::true_type {};
template<typename T> struct is_filter<Optional<T>> : std::true_type {};

template<typename T> struct extract_type { using type = T; };
template<typename T> struct extract_type<With<T>> { using type = T; };
template<typename T> struct extract_type<Without<T>> { using type = T; };
template<typename T> struct extract_type<Optional<T>> { using type = T; };

export template<typename... Components>
class Query {
private:
    std::tuple<ComponentStorage<Components>*...> m_Storages;
    EntityID m_MaxEntity;

    [[nodiscard]] bool HasAllComponents(EntityID entity) const {
        return (std::get<ComponentStorage<Components>*>(m_Storages)->Contains(entity) && ...);
    }

public:
    Query(ComponentStorage<Components>*... storages, EntityID maxEntity)
        : m_Storages{storages...}
        , m_MaxEntity{maxEntity} {}

    class Iterator {
    private:
        const Query* m_Query;
        EntityID m_Current;

        void AdvanceToValid() {
            while (m_Current < m_Query->m_MaxEntity &&
                   !m_Query->HasAllComponents(m_Current)) {
                ++m_Current;
            }
        }

    public:
        Iterator(const Query* query, EntityID start)
            : m_Query{query}, m_Current{start} {
            AdvanceToValid();
        }

        bool operator!=(const Iterator& other) const {
            return m_Current != other.m_Current;
        }

        bool operator==(const Iterator& other) const {
            return m_Current == other.m_Current;
        }

        Iterator& operator++() {
            ++m_Current;
            AdvanceToValid();
            return *this;
        }

        auto operator*() const {
            return std::apply([this](auto*... storages) {
                return std::tuple<Components&...>(*storages->GetUnchecked(m_Current)...);
            }, m_Query->m_Storages);
        }

        EntityID Entity() const { return m_Current; }
    };

    Iterator begin() const { return Iterator{this, 0}; }
    Iterator end() const { return Iterator{this, m_MaxEntity}; }

    [[nodiscard]] auto Get(EntityID entity) const
        -> std::optional<std::tuple<Components&...>> {
        if (!HasAllComponents(entity)) {
            return std::nullopt;
        }

        return std::apply([entity](auto*... storages) {
            return std::make_optional(std::tuple<Components&...>(*storages->GetUnchecked(entity)...));
        }, m_Storages);
    }

    [[nodiscard]] bool IsEmpty() const {
        return begin() == end();
    }

    [[nodiscard]] std::size_t Count() const {
        std::size_t count = 0;
        for (auto it = begin(); it != end(); ++it) {
            ++count;
        }
        return count;
    }

    template<typename Func>
    void ForEach(Func&& func) const {
        for (auto components : *this) {
            std::apply(func, components);
        }
    }

    template<typename Func>
    void ParForEach(Func&& func) const {
        std::vector<EntityID> validEntities;
        validEntities.reserve(1000);

        for (EntityID e = 0; e < m_MaxEntity; ++e) {
            if (HasAllComponents(e)) {
                validEntities.push_back(e);
            }
        }

        std::for_each(std::execution::par_unseq,
            validEntities.begin(), validEntities.end(),
            [this, &func](EntityID entity) {
                std::apply([entity, &func](auto*... storages) {
                    func(*storages->GetUnchecked(entity)...);
                }, m_Storages);
            }
        );
    }
};

export template<typename... Components>
class FilteredQuery {
private:
    std::tuple<ComponentStorage<Components>*...> m_RequiredStorages;
    std::vector<IComponentStorageBase*> m_WithStorages;
    std::vector<IComponentStorageBase*> m_WithoutStorages;
    std::vector<IComponentStorageBase*> m_OptionalStorages;
    EntityID m_MaxEntity;

    [[nodiscard]] bool Matches(EntityID entity) const {
        auto hasRequired = std::apply([entity](auto*... storages) {
            return (storages->Contains(entity) && ...);
        }, m_RequiredStorages);

        if (!hasRequired) return false;

        for (auto* storage : m_WithStorages) {
            if (!storage->Contains(entity)) return false;
        }

        for (auto* storage : m_WithoutStorages) {
            if (storage->Contains(entity)) return false;
        }

        return true;
    }

public:
    FilteredQuery(std::tuple<ComponentStorage<Components>*...> required,
                  std::vector<IComponentStorageBase*> with,
                  std::vector<IComponentStorageBase*> without,
                  std::vector<IComponentStorageBase*> optional,
                  EntityID maxEntity)
        : m_RequiredStorages{required}
        , m_WithStorages{std::move(with)}
        , m_WithoutStorages{std::move(without)}
        , m_OptionalStorages{std::move(optional)}
        , m_MaxEntity{maxEntity} {}

    class Iterator {
    private:
        const FilteredQuery* m_Query;
        EntityID m_Current;

        void AdvanceToValid() {
            while (m_Current < m_Query->m_MaxEntity &&
                   !m_Query->Matches(m_Current)) {
                ++m_Current;
            }
        }

    public:
        Iterator(const FilteredQuery* query, EntityID start)
            : m_Query{query}, m_Current{start} {
            AdvanceToValid();
        }

        bool operator!=(const Iterator& other) const {
            return m_Current != other.m_Current;
        }

        Iterator& operator++() {
            ++m_Current;
            AdvanceToValid();
            return *this;
        }

        auto operator*() const {
            return std::apply([this](auto*... storages) {
                return std::tuple<Components&...>(*storages->GetUnchecked(m_Current)...);
            }, m_Query->m_RequiredStorages);
        }

        EntityID Entity() const { return m_Current; }
    };

    Iterator begin() const { return Iterator{this, 0}; }
    Iterator end() const { return Iterator{this, m_MaxEntity}; }
};

export template<typename World, typename... Components>
[[nodiscard]] auto MakeQuery(World* world) {
    return Query<Components...>{
        world->template GetStorage<Components>()...,
        world->GetMaxEntityId()
    };
}

export template<typename World>
class QueryBuilder {
private:
    World* m_World;
    std::vector<IComponentStorageBase*> m_With;
    std::vector<IComponentStorageBase*> m_Without;
    std::vector<IComponentStorageBase*> m_Optional;

public:
    explicit QueryBuilder(World* world) : m_World{world} {}

    template<typename T>
    QueryBuilder& With() {
        m_With.push_back(m_World->template GetStorage<T>());
        return *this;
    }

    template<typename T>
    QueryBuilder& Without() {
        m_Without.push_back(m_World->template GetStorage<T>());
        return *this;
    }

    template<typename T>
    QueryBuilder& Optional() {
        m_Optional.push_back(m_World->template GetStorage<T>());
        return *this;
    }

    template<typename... Components>
    auto Build() {
        return FilteredQuery<Components...>{
            std::make_tuple(m_World->template GetStorage<Components>()...),
            m_With, m_Without, m_Optional,
            m_World->GetMaxEntityId()
        };
    }
};
