export module ECS.Query;

import ECS.Component;
import Core.Types;
import std;

export template<typename T>
struct With {
    using type = T;
};

export template<typename T>
struct Without {
    using type = T;
};

export template<typename T>
struct Optional {
    using type = T;
};

template<typename T>
struct is_with : std::false_type {};
template<typename T>
struct is_with<With<T>> : std::true_type {};

template<typename T>
struct is_without : std::false_type {};
template<typename T>
struct is_without<Without<T>> : std::true_type {};

template<typename T>
struct is_optional : std::false_type {};
template<typename T>
struct is_optional<Optional<T>> : std::true_type {};

template<typename T>
struct extract_type {
    using type = T;
};
template<typename T>
struct extract_type<With<T>> {
    using type = T;
};
template<typename T>
struct extract_type<Without<T>> {
    using type = T;
};
template<typename T>
struct extract_type<Optional<T>> {
    using type = T;
};

export template<typename World, typename... Args>
class Query {
private:
    World* m_World;
    std::vector<EntityID> m_Entities;

    template<typename T>
    bool ContainsComponent(EntityID entity) const {
        auto* storage = m_World->template GetStorage<typename extract_type<T>::type>();
        return storage && storage->Contains(entity);
    }

    template<typename Filter>
    bool CheckFilter(EntityID entity) const {
        if constexpr (is_with<Filter>::value) {
            return ContainsComponent<typename Filter::type>(entity);
        } else if constexpr (is_without<Filter>::value) {
            return !ContainsComponent<typename Filter::type>(entity);
        } else if constexpr (is_optional<Filter>::value) {
            return true;
        } else {
            return ContainsComponent<Filter>(entity);
        }
    }

    void CollectEntities() {
        auto maxEntity = m_World->GetMaxEntityId();
        for (EntityID entity = 0; entity < maxEntity; ++entity) {
            if ((CheckFilter<Args>(entity) && ...)) {
                m_Entities.push_back(entity);
            }
        }
    }

    template<typename... Components>
    struct ComponentGetter {
        World* world;

        template<typename T>
        auto GetOne(EntityID entity) const {
            if constexpr (std::disjunction_v<std::is_same<Optional<T>, Args>...>) {
                auto* storage = world->template GetStorage<T>();
                return storage && storage->Contains(entity) ? static_cast<T*>(storage->GetRaw(entity)) : nullptr;
            } else {
                return static_cast<T*>(world->template GetStorage<T>()->GetRaw(entity));

            }
        }

        auto Get(EntityID entity) const {
            return std::tuple<decltype(GetOne<Components>(entity))...>(GetOne<Components>(entity)...);
        }
    };

public:
    explicit Query(World* world) : m_World(world) {
        CollectEntities();
    }

    template<typename... Components>
    class TypedIterator {
    private:
        Query* m_Query;
        size_t m_Index;
        ComponentGetter<Components...> m_Getter;

    public:
        TypedIterator(Query* query, size_t index)
            : m_Query(query), m_Index(index), m_Getter{query->m_World} {}

        auto operator*() const {
            EntityID entity = m_Query->m_Entities[m_Index];
            return m_Getter.Get(entity);
        }

        TypedIterator& operator++() {
            ++m_Index;
            return *this;
        }

        bool operator!=(const TypedIterator& other) const {
            return m_Index != other.m_Index;
        }
    };

    template<typename... Components>
    auto Iter() {
        struct Range {
            Query* query;
            auto begin() { return TypedIterator<Components...>(query, 0); }
            auto end() { return TypedIterator<Components...>(query, query->m_Entities.size()); }
        };
        return Range{this};
    }

    [[nodiscard]] size_t Count() const { return m_Entities.size(); }
    [[nodiscard]] bool IsEmpty() const { return m_Entities.empty(); }

    template<typename... Components>
    void ForEach(auto func) {
        for (auto entity : m_Entities) {
            ComponentGetter<Components...> getter{m_World};
            std::apply(func, getter.Get(entity));
        }
    }

    template<typename... Components>
    void ParForEach(auto func) {
        std::for_each(std::execution::par_unseq, m_Entities.begin(), m_Entities.end(),
            [this, &func](EntityID entity) {
                ComponentGetter<Components...> getter{m_World};
                std::apply(func, getter.Get(entity));
            });
    }
};