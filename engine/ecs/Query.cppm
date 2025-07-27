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

    template<typename T>
    [[nodiscard]] bool ContainsComponent(EntityID entity) const {
        auto* storage = m_World->template GetStorage<typename extract_type<T>::type>();
        return storage && storage->Contains(entity);
    }

    template<typename Filter>
    [[nodiscard]] bool CheckFilter(EntityID entity) const {
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

    [[nodiscard]] bool MatchQuery(EntityID entity) const {
        return (CheckFilter<Args>(entity) && ...);
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
    explicit Query(World* world) : m_World{world} {}

    template<typename... Components>
    class TypedIterator {
    private:
        Query* m_Query;
        EntityID m_Current;
        EntityID m_Max;
        ComponentGetter<Components...> m_Getter;

        void FindNext() {
            while (m_Current < m_Max && !m_Query->MatchQuery(m_Current)) {
                ++m_Current;
            }
        }
    public:
        TypedIterator(Query* query, EntityID start, EntityID max) : m_Query{query}, m_Current{start}, m_Max{max}, m_Getter{query->m_World} {
            if (m_Current < m_Max) {
                FindNext();
            }
        }

        auto operator*() const {
            return m_Getter.Get(m_Current);
        }

        TypedIterator& operator++() {
            ++m_Current;
            FindNext();
            return *this;
        }

        bool operator!=(const TypedIterator& other) const {
            return m_Current != other.m_Current;
        }
    };

    template<typename... Components>
    auto Iter() {
        struct Range {
            Query* query;
            EntityID maxEntity;
            auto begin() { return TypedIterator<Components...>(query, 0, maxEntity); }
            auto end() { return TypedIterator<Components...>(query, maxEntity, maxEntity); }
        };
        return Range{this, m_World->GetMaxEntityId()};
    }

    [[nodiscard]] size_t Count() const {
        size_t count = 0;
        auto maxEntity = m_World->GetMaxEntityId();
        for (EntityID entity = 0; entity < maxEntity; ++entity) {
            if (MatchQuery(entity)) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool IsEmpty() const {
        auto maxEntity = m_World->GetMaxEntityId();
        for (EntityID entity = 0; entity < maxEntity; ++entity) {
            if (MatchQuery(entity)) {
                return false;
            }
        }
        return true;
    }

    template<typename... Components>
    void ForEach(auto func) {
        ComponentGetter<Components...> getter{m_World};
        auto maxEntity = m_World->GetMaxEntityId();
        for (EntityID entity = 0; entity < maxEntity; ++entity) {
            if (MatchQuery(entity)) {
                std::apply(func, getter.Get(entity));
            }
        }
    }
};