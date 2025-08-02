export module ECS.Query;

import ECS.Component;
import Core.Types;
import std;

// Component access modifiers
export template<typename T>
struct Read {
    using type = T;
    static constexpr bool is_read = true;
    static constexpr bool is_write = false;
};

export template<typename T>
struct Write {
    using type = T;
    static constexpr bool is_read = false;
    static constexpr bool is_write = true;
};

export template<typename T>
struct ReadWrite {
    using type = T;
    static constexpr bool is_read = true;
    static constexpr bool is_write = true;
};

export template<typename T>
struct With {
    using type = T;
    static constexpr bool is_with = true;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = true;
    static constexpr bool is_write = true;
};

export template<typename T>
struct Without {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = true;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = false;
    static constexpr bool is_write = false;
};

export template<typename T>
struct Optional {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = true;
    static constexpr bool is_read = true;
    static constexpr bool is_write = false;
};

template<typename T>
struct extract_type {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = true;
    static constexpr bool is_write = true;
    static constexpr bool is_component = true;
};

template<typename T>
struct extract_type<Read<T>> {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = true;
    static constexpr bool is_write = false;
    static constexpr bool is_component = true;
};

template<typename T>
struct extract_type<Write<T>> {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = false;
    static constexpr bool is_write = true;
    static constexpr bool is_component = true;
};

template<typename T>
struct extract_type<ReadWrite<T>> {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = true;
    static constexpr bool is_write = true;
    static constexpr bool is_component = true;
};

template<typename T>
struct extract_type<With<T>> {
    using type = T;
    static constexpr bool is_with = true;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = true;
    static constexpr bool is_write = true;
    static constexpr bool is_component = false;
};

template<typename T>
struct extract_type<Without<T>> {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = true;
    static constexpr bool is_optional = false;
    static constexpr bool is_read = false;
    static constexpr bool is_write = false;
    static constexpr bool is_component = false;
};

template<typename T>
struct extract_type<Optional<T>> {
    using type = T;
    static constexpr bool is_with = false;
    static constexpr bool is_without = false;
    static constexpr bool is_optional = true;
    static constexpr bool is_read = true;
    static constexpr bool is_write = false;
    static constexpr bool is_component = true;
};

template<typename... Args>
struct QueryMasks {
    static consteval auto Calculate() {
        struct Masks {
            Archetype include{0};
            Archetype exclude{0};
            Archetype optional{0};
            Archetype reads{0};
            Archetype writes{0};
        } masks;

        auto processArg = []<typename Arg>(Masks& m) {
            using Extracted = extract_type<Arg>;
            const ComponentID id = ComponentRegistry::GetID<typename Extracted::type>();

            if constexpr (Extracted::is_component || Extracted::is_with) {
                m.include |= (1ULL << id);
                if constexpr (Extracted::is_read) {
                    m.reads |= (1ULL << id);
                }
                if constexpr (Extracted::is_write) {
                    m.writes |= (1ULL << id);
                }
            } else if constexpr (Extracted::is_without) {
                m.exclude |= (1ULL << id);
            } else if constexpr (Extracted::is_optional) {
                m.optional |= (1ULL << id);
                if constexpr (Extracted::is_read) {
                    m.reads |= (1ULL << id);
                }
                if constexpr (Extracted::is_write) {
                    m.writes |= (1ULL << id);
                }
            }
        };

        (processArg.template operator()<Args>(masks), ...);
        return masks;
    }

    static constexpr auto masks = Calculate();
};

export template<typename World, typename... Args>
class Query {
private:
    World* m_World;

    static constexpr auto s_Masks = QueryMasks<Args...>::masks;

    [[nodiscard]] constexpr bool MatchesArchetype(Archetype arch) const {
        if ((arch & s_Masks.include) != s_Masks.include) return false;
        if ((arch & s_Masks.exclude) != 0) return false;
        return true;
    }

    [[nodiscard]] Archetype GetEntityArchetype(EntityHandle handle) const {
        return m_World->GetEntityArchetype(handle);
    }

public:
    explicit Query(World* world) : m_World{world} {}

    template<typename... Components>
    struct ComponentGetter {
        World* world;

        template<typename T>
        auto GetOne(EntityHandle handle) const {
            constexpr bool isOptional = []<typename... QArgs>() {
                return ((std::is_same_v<Optional<T>, QArgs> || ...) || false);
            }.template operator()<Args...>();

            if constexpr (isOptional) {
                auto* storage = world->template GetStorage<T>();
                return storage ? storage->Get(handle) : nullptr;
            } else {
                return world->template GetStorage<T>()->Get(handle);
            }
        }

        auto Get(EntityHandle handle) const {
            return std::tuple<decltype(GetOne<Components>(handle))...>(
                GetOne<Components>(handle)...
            );
        }
    };

    template<typename... Components>
    class TypedIterator {
    private:
        Query* m_Query;
        World* m_World;
        typename World::EntityIterator m_Current;
        typename World::EntityIterator m_End;
        ComponentGetter<Components...> m_Getter;

        void FindNext() {
            while (m_Current != m_End) {
                auto [handle, arch] = *m_Current;
                if (m_Query->MatchesArchetype(arch)) {
                    break;
                }
                ++m_Current;
            }
        }

    public:
        TypedIterator(Query* query, typename World::EntityIterator begin,
                     typename World::EntityIterator end)
            : m_Query{query}, m_World{query->m_World},
              m_Current{begin}, m_End{end}, m_Getter{query->m_World} {
            FindNext();
        }

        auto operator*() const {
            return m_Getter.Get((*m_Current).first);
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

            auto begin() {
                return TypedIterator<Components...>(
                    query, query->m_World->EntitiesBegin(),
                    query->m_World->EntitiesEnd()
                );
            }

            auto end() {
                return TypedIterator<Components...>(
                    query, query->m_World->EntitiesEnd(),
                    query->m_World->EntitiesEnd()
                );
            }
        };
        return Range{this};
    }

    [[nodiscard]] USize Count() const {
        USize count = 0;
        for (auto it = m_World->EntitiesBegin(); it != m_World->EntitiesEnd(); ++it) {
            if (MatchesArchetype(it->second)) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool IsEmpty() const {
        for (auto it = m_World->EntitiesBegin(); it != m_World->EntitiesEnd(); ++it) {
            if (MatchesArchetype(it->second)) {
                return false;
            }
        }
        return true;
    }

    template<typename Func>
    void ForEach(Func func) {
        for (auto it = m_World->EntitiesBegin(); it != m_World->EntitiesEnd(); ++it) {
            if (MatchesArchetype(it->second)) {
                ForEachImpl(it->first, func, std::make_index_sequence<sizeof...(Args)>{});
            }
        }
    }

    static consteval Archetype GetIncludeMask() { return s_Masks.include; }
    static consteval Archetype GetExcludeMask() { return s_Masks.exclude; }
    static consteval Archetype GetOptionalMask() { return s_Masks.optional; }
    static consteval Archetype GetReadMask() { return s_Masks.reads; }
    static consteval Archetype GetWriteMask() { return s_Masks.writes; }

private:
    template<typename Func, size_t... Is>
    void ForEachImpl(EntityHandle handle, Func func, std::index_sequence<Is...>) {
        auto args = std::tuple_cat(GetComponentIfNeeded<Args>(handle)...);
        std::apply(func, args);
    }

    template<typename Arg>
    auto GetComponentIfNeeded(EntityHandle handle) {
        using T = typename extract_type<Arg>::type;

        if constexpr (!extract_type<Arg>::is_component) {
            // With/Without don't contribute to the argument list
            return std::tuple<>();
        } else if constexpr (extract_type<Arg>::is_optional) {
            // Optional components
            auto* storage = m_World->template GetStorage<T>();
            auto* component = storage ? storage->Get(handle) : nullptr;
            return std::make_tuple(component);
        } else if constexpr (extract_type<Arg>::is_read && !extract_type<Arg>::is_write) {
            // Read-only components - return const pointer
            auto* component = m_World->template GetStorage<T>()->Get(handle);
            return std::make_tuple(static_cast<const T*>(component));
        } else {
            // Write or ReadWrite components - return non-const pointer
            auto* component = m_World->template GetStorage<T>()->Get(handle);
            return std::make_tuple(component);
        }
    }
};