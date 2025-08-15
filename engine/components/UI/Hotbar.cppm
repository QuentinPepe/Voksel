export module Systems.Hotbar;

import ECS.Component;
import ECS.SystemScheduler;
import ECS.World;
import Components.Voxel;
import Input.Core;
import Input.Manager;
import UI.Core;
import UI.Element;
import UI.Layout;
import UI.Widgets;
import UI.Manager;
import Core.Types;
import Core.Assert;
import Math.Core;
import Math.Vector;
import std;

export class HotbarSystem : public System<HotbarSystem> {
private:
    UIManager* m_UI{nullptr};
    InputManager* m_Input{nullptr};
    UIElementPtr m_Container{};
    UIElementPtr m_Layout{};

    struct Slot {
        UIElementPtr panel;
        UIElementPtr image;
        UIElementPtr countText;
        UIElementPtr selectionBorder;
        Voxel voxel;
        U32 count;
    };
    Vector<Slot> m_Slots{};

    EntityHandle m_StateEntity{};
    U32 m_Selected{0};
    bool m_Built{false};

public:
    void Setup(){ SetName("Hotbar"); SetStage(SystemStage::PreUpdate); SetPriority(SystemPriority::High); SetParallel(false); }
    void SetUIManager(UIManager* ui){ m_UI = ui; }
    void SetInputManager(InputManager* im){ m_Input = im; }

    void OnResize(U32 w,U32 h){
        if (!m_Container || !m_Layout || m_Slots.empty()) return;
        constexpr F32 kSlot{44.0f}; constexpr F32 kBaseSpacing{5.0f}; constexpr F32 kBasePadding{5.0f};
        U32 kCount{static_cast<U32>(m_Slots.size())};
        F32 baseW{kCount*kSlot+(kCount-1)*kBaseSpacing+2.0f*kBasePadding};
        F32 maxW{std::max(0.0f, static_cast<F32>(w)-40.0f)};
        F32 scale{std::min(1.0f, maxW/baseW)};
        F32 spacing{kBaseSpacing*scale}; F32 padding{kBasePadding*scale}; F32 slot{kSlot*scale};
        F32 contW{kCount*slot+(kCount-1)*spacing+2.0f*padding}; F32 contH{60.0f*scale};
        m_Container->SetSizeDelta({contW, contH});
        m_Layout->SetMargin(Margin{padding});
        std::static_pointer_cast<UIHorizontalLayout>(m_Layout)->SetSpacing(spacing);
        for (auto& s : m_Slots){
            std::static_pointer_cast<UIPanel>(s.panel)->SetBorderWidth(2.0f*scale);
            s.panel->SetSizeDelta({slot,slot});
            if (s.image) s.image->SetSizeDelta({32.0f*scale,32.0f*scale});
            s.countText->SetAnchoredPosition({-2.0f*scale,-2.0f*scale});
            std::static_pointer_cast<UIText>(s.countText)->SetFontSize(static_cast<U32>(std::max(8.0f,10.0f*scale)));
            std::static_pointer_cast<UIPanel>(s.selectionBorder)->SetBorderWidth(3.0f*scale);
        }
    }

    void Run(World* world, F32) override{
        if (!m_UI || !m_Input) return;
        if (!m_Built) BuildIfNeeded(world);
        if (!m_Container) return;

        if (m_Input->IsKeyJustPressed(Key::H)){
            bool vis{m_Container->IsVisible()};
            m_Container->SetVisibility(vis?Visibility::Hidden:Visibility::Visible);
        }

        U32 count{static_cast<U32>(m_Slots.size())};
        for (U32 i{}; i<count; ++i)
            if (m_Input->IsKeyJustPressed(static_cast<Key>(static_cast<U16>(Key::Num1)+i))) ApplySelection(world,i);

        F32 scroll{static_cast<F32>(m_Input->GetMouseScroll().second)};
        if (scroll!=0.0f && count>0){
            if (scroll>0) ApplySelection(world,(m_Selected+1)%count);
            else ApplySelection(world,(m_Selected+count-1)%count);
        }
    }

private:
    static Math::Vec4 TileUV(U32 tile, VoxelAtlasInfo const& ai){
        U32 tx{tile % ai.tilesX};
        U32 ty{tile / ai.tilesX};
        U32 cell{ai.tileSize + 2u*ai.pad};
        U32 px0{tx*cell + ai.pad};
        U32 py0{ty*cell + ai.pad};
        U32 px1{px0 + ai.tileSize};
        U32 py1{py0 + ai.tileSize};
        F32 u0{static_cast<F32>(px0)/ai.atlasW};
        F32 v0{static_cast<F32>(py0)/ai.atlasH};
        F32 u1{static_cast<F32>(px1)/ai.atlasW};
        F32 v1{static_cast<F32>(py1)/ai.atlasH};
        return Math::Vec4{u0,v0,u1,v1};
    }

    void BuildIfNeeded(World* world){
        assert(m_UI!=nullptr, "UIManager must be set");
        if (m_Built) return;

        VoxelAtlasInfo const* ai{};
        if (auto* s{world->GetStorage<VoxelAtlasInfo>()})
            for (auto [h,c] : *s){ ai=&c; break; }
        if (!ai || ai->texture == INVALID_INDEX) {
            return;
        }

        auto root{m_UI->GetRoot()};
        assert(root!=nullptr, "UI root missing");

        m_Container = m_UI->CreatePanel("HotbarContainer");
        m_Container->SetAnchor(AnchorPreset::BottomCenter);
        m_Container->SetPivot({0.5f,1.0f});
        m_Container->SetAnchoredPosition({0.0f,-20.0f});
        m_Container->SetSizeDelta({450.0f,60.0f});
        root->AddChild(m_Container);

        m_Layout = m_UI->CreateHorizontalLayout();
        m_Layout->SetAnchor(AnchorPreset::StretchAll);
        m_Layout->SetMargin(Margin{5.0f});
        std::static_pointer_cast<UIHorizontalLayout>(m_Layout)->SetSpacing(5.0f);
        std::static_pointer_cast<UIHorizontalLayout>(m_Layout)->SetChildControl(false,true);
        std::static_pointer_cast<UIHorizontalLayout>(m_Layout)->SetChildAlignment(Alignment::Center);
        m_Container->AddChild(m_Layout);

        struct Item{ Voxel v; U32 tile; };
        Vector<Item> items{
            Item{Voxel::Dirt,   AtlasTileFor(Voxel::Dirt)},
            Item{Voxel::Grass,  AtlasTileFor(Voxel::Grass)},
            Item{Voxel::Stone,  AtlasTileFor(Voxel::Stone)},
            Item{Voxel::Log,    AtlasTileFor(Voxel::Log)},
            Item{Voxel::Leaves, AtlasTileFor(Voxel::Leaves)}
        };

        m_Slots.clear();
        for (USize i{}; i<items.size(); ++i){
            auto panel{m_UI->CreatePanel()};
            panel->SetSizeDelta({44.0f,44.0f});
            std::static_pointer_cast<UIPanel>(panel)->SetBackgroundColor(Color{0.15f,0.15f,0.15f,0.9f});
            std::static_pointer_cast<UIPanel>(panel)->SetBorderWidth(2.0f);
            std::static_pointer_cast<UIPanel>(panel)->SetBorderColor(Color{0.3f,0.3f,0.3f,1.0f});

            auto img = std::static_pointer_cast<UIImage>(m_UI->CreateImage());
            img->SetAnchor(AnchorPreset::Center);
            img->SetPivot({0.5f,0.5f});
            img->SetSizeDelta({32.0f,32.0f});
            img->SetTexture(ai->texture);
            img->SetUVRect(TileUV(items[i].tile, *ai));
            panel->AddChild(img);

            auto countText{m_UI->CreateText("64")};
            countText->SetAnchor(AnchorPreset::BottomRight);
            countText->SetPivot({1.0f,1.0f});
            countText->SetAnchoredPosition({-2.0f,-2.0f});
            countText->SetSizeDelta({20.0f,12.0f});
            std::static_pointer_cast<UIText>(countText)->SetFontSize(10);
            std::static_pointer_cast<UIText>(countText)->SetTextColor(Color::White);
            std::static_pointer_cast<UIText>(countText)->SetAlignment(Alignment::End,Alignment::End);
            panel->AddChild(countText);

            auto selection{m_UI->CreatePanel()};
            selection->SetAnchor(AnchorPreset::StretchAll);
            selection->SetMargin(Margin{-2.0f});
            std::static_pointer_cast<UIPanel>(selection)->SetBackgroundColor(Color::Transparent);
            std::static_pointer_cast<UIPanel>(selection)->SetBorderWidth(3.0f);
            std::static_pointer_cast<UIPanel>(selection)->SetBorderColor(Color{1.0f,1.0f,0.0f,1.0f});
            selection->SetVisibility(i==0?Visibility::Visible:Visibility::Hidden);
            panel->AddChild(selection);

            m_Layout->AddChild(panel);

            Slot s{};
            s.panel = panel; s.image = img; s.countText = countText; s.selectionBorder = selection;
            s.voxel = items[i].v; s.count = 64;
            m_Slots.push_back(s);
        }

        VoxelHotbarState hb{};
        hb.slots.reserve(m_Slots.size());
        for (auto const& s : m_Slots) hb.slots.push_back(s.voxel);
        hb.selectedIndex = 0u; hb.selected = hb.slots[hb.selectedIndex];

        auto* store{world->GetStorage<VoxelHotbarState>()};
        if (!store || store->Size()==0){ m_StateEntity = world->CreateEntity(); world->AddComponent(m_StateEntity, hb); }
        else { for (auto [h,st] : *store){ m_StateEntity=h; break; } }

        m_Built = true;
    }

    void ApplySelection(World* world, U32 idx){
        if (m_Slots.empty()) return;
        idx = std::min<U32>(idx, static_cast<U32>(m_Slots.size()-1));
        m_Slots[m_Selected].selectionBorder->SetVisibility(Visibility::Hidden);
        m_Selected = idx;
        m_Slots[m_Selected].selectionBorder->SetVisibility(Visibility::Visible);
        auto* st{world->GetComponent<VoxelHotbarState>(m_StateEntity)};
        if (!st) return;
        st->selectedIndex = m_Selected; st->selected = m_Slots[m_Selected].voxel;
    }
};
