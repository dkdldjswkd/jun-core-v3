#pragma once
#include "GameObject.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <utility>
#include <functional>

//------------------------------
// AoiGrid - 격자 기반 AOI (Area of Interest) 시스템
//
// 맵을 일정 크기의 셀(Cell)로 분할하고,
// 오브젝트의 위치 변경 시 인접 9셀 기준으로
// OnAppear / OnDisappear 이벤트를 발행한다.
//
// Hysteresis:
//   경계 근처에서의 깜빡임 방지.
//   현재 셀의 경계를 hysteresisBuffer만큼 확장한 범위를
//   벗어나야 셀 이동을 인정한다.
//------------------------------
class AoiGrid
{
public:
    AoiGrid(float minX, float minZ, float maxX, float maxZ,
            float cellSize, float hysteresisBuffer)
        : m_minX(minX), m_minZ(minZ)
        , m_cellSize(cellSize)
        , m_hysteresisBuffer(hysteresisBuffer)
    {
        m_cols = static_cast<int>(std::ceil((maxX - minX) / cellSize));
        m_rows = static_cast<int>(std::ceil((maxZ - minZ) / cellSize));
        if (m_cols < 1) m_cols = 1;
        if (m_rows < 1) m_rows = 1;
        m_cells.resize(static_cast<size_t>(m_rows * m_cols));
    }

    //------------------------------
    // 오브젝트를 그리드에 추가 (Scene Enter 시)
    // appear/disappear 알림은 GameScene::Enter에서 OnAppear 경유로 처리
    //------------------------------
    void AddObject(GameObject* obj)
    {
        int col = WorldToCol(obj->GetX());
        int row = WorldToRow(obj->GetZ());
        ClampCell(row, col);

        m_objectCellMap[obj] = {row, col};
        m_cells[CellIndex(row, col)].objects.insert(obj);
    }

    //------------------------------
    // 오브젝트를 그리드에서 제거 (Scene Exit 시)
    // 주변 오브젝트들에게 OnDisappear 이벤트 발행
    //------------------------------
    void RemoveObject(GameObject* obj)
    {
        auto it = m_objectCellMap.find(obj);
        if (it == m_objectCellMap.end())
        {
            return;
        }

        auto [row, col] = it->second;

        // 주변 오브젝트들에게 내가 사라짐을 알림
        std::vector<GameObject*> nearby = CollectAdjacent(row, col, obj);
        if (!nearby.empty())
        {
            std::vector<GameObject*> me_list = { obj };
            for (auto* other : nearby)
            {
                other->OnDisappear(me_list);
            }
            // 나에게 주변 오브젝트들이 사라짐 (씬을 나가므로 자신의 OnDisappear도 호출)
            obj->OnDisappear(nearby);
        }

        m_cells[CellIndex(row, col)].objects.erase(obj);
        m_objectCellMap.erase(it);
    }

    //------------------------------
    // 오브젝트 위치 갱신 (MoveComponent에서 호출)
    // Hysteresis 판정 후 셀 이동 시 OnAppear / OnDisappear 발행
    //------------------------------
    void UpdatePosition(GameObject* obj, float x, float z)
    {
        // 잦은 주기로 호출되는 함수인데 매번 Find??
        auto it = m_objectCellMap.find(obj);
        if (it == m_objectCellMap.end())
        {
            return;
        }

        auto [oldRow, oldCol] = it->second;

        // 현재 커밋된 셀의 Hysteresis 경계 계산
        float cellMinX = m_minX + static_cast<float>(oldCol) * m_cellSize;
        float cellMaxX = cellMinX + m_cellSize;
        float cellMinZ = m_minZ + static_cast<float>(oldRow) * m_cellSize;
        float cellMaxZ = cellMinZ + m_cellSize;

        bool outsideHysteresis =
            x < cellMinX - m_hysteresisBuffer ||
            x >= cellMaxX + m_hysteresisBuffer ||
            z < cellMinZ - m_hysteresisBuffer ||
            z >= cellMaxZ + m_hysteresisBuffer;

        if (!outsideHysteresis)
        {
            // Hysteresis 경계 내 → 셀 변경 없음
            return;
        }

        // 새 셀 계산
        int newCol = WorldToCol(x);
        int newRow = WorldToRow(z);
        ClampCell(newRow, newCol);

        if (newRow == oldRow && newCol == oldCol)
        {
            return;
        }

        // 이전 인접 셀의 오브젝트 집합
        std::unordered_set<GameObject*> oldAdjacent;
        CollectAdjacentSet(oldRow, oldCol, obj, oldAdjacent);

        // 셀 이동
        m_cells[CellIndex(oldRow, oldCol)].objects.erase(obj);
        m_cells[CellIndex(newRow, newCol)].objects.insert(obj);
        it->second = { newRow, newCol };

        // 새 인접 셀의 오브젝트 집합
        std::unordered_set<GameObject*> newAdjacent;
        CollectAdjacentSet(newRow, newCol, obj, newAdjacent);

        // 새로 보이게 된 오브젝트 (newAdjacent - oldAdjacent)
        std::vector<GameObject*> appeared;
        for (auto* o : newAdjacent)
        {
            if (oldAdjacent.find(o) == oldAdjacent.end())
            {
                appeared.push_back(o);
            }
        }

        // 더 이상 안 보이는 오브젝트 (oldAdjacent - newAdjacent)
        std::vector<GameObject*> disappeared;
        for (auto* o : oldAdjacent)
        {
            if (newAdjacent.find(o) == newAdjacent.end())
            {
                disappeared.push_back(o);
            }
        }

        // Appear 통지 (양방향)
        if (!appeared.empty())
        {
            obj->OnAppear(appeared);
            std::vector<GameObject*> me_list = { obj };
            for (auto* o : appeared)
            {
                o->OnAppear(me_list);
            }
        }

        // Disappear 통지 (양방향)
        if (!disappeared.empty())
        {
            obj->OnDisappear(disappeared);
            std::vector<GameObject*> me_list = { obj };
            for (auto* o : disappeared)
            {
                o->OnDisappear(me_list);
            }
        }
    }

    //------------------------------
    // 인접 9셀의 오브젝트 반환
    // excludeSelf가 지정되면 해당 오브젝트 제외
    //------------------------------
    std::vector<GameObject*> GetNearbyObjects(float x, float z, GameObject* excludeSelf = nullptr) const
    {
        int col = WorldToCol(x);
        int row = WorldToRow(z);
        ClampCell(row, col);

        std::vector<GameObject*> result;
        for (int dr = -1; dr <= 1; ++dr)
        {
            for (int dc = -1; dc <= 1; ++dc)
            {
                int r = row + dr;
                int c = col + dc;
                if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
                {
                    continue;
                }
                for (auto* o : m_cells[CellIndex(r, c)].objects)
                {
                    if (o != excludeSelf)
                    {
                        result.push_back(o);
                    }
                }
            }
        }
        return result;
    }

    //------------------------------
    // 인접 9셀의 오브젝트 순회
    //------------------------------
    void ForEachAdjacentObjects(float x, float z, const std::function<void(GameObject*)>& fn) const
    {
        int col = WorldToCol(x);
        int row = WorldToRow(z);
        ClampCell(row, col);

        for (int dr = -1; dr <= 1; ++dr)
        {
            for (int dc = -1; dc <= 1; ++dc)
            {
                int r = row + dr;
                int c = col + dc;
                if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
                {
                    continue;
                }
                for (auto* o : m_cells[CellIndex(r, c)].objects)
                {
                    fn(o);
                }
            }
        }
    }

private:
    struct Cell
    {
        std::unordered_set<GameObject*> objects;
    };

    int WorldToCol(float x) const
    {
        return static_cast<int>((x - m_minX) / m_cellSize);
    }

    int WorldToRow(float z) const
    {
        return static_cast<int>((z - m_minZ) / m_cellSize);
    }

    void ClampCell(int& row, int& col) const
    {
        if (row < 0) row = 0;
        if (row >= m_rows) row = m_rows - 1;
        if (col < 0) col = 0;
        if (col >= m_cols) col = m_cols - 1;
    }

    int CellIndex(int row, int col) const
    {
        return row * m_cols + col;
    }

    std::vector<GameObject*> CollectAdjacent(int row, int col, GameObject* exclude) const
    {
        std::vector<GameObject*> result;
        for (int dr = -1; dr <= 1; ++dr)
        {
            for (int dc = -1; dc <= 1; ++dc)
            {
                int r = row + dr;
                int c = col + dc;
                if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
                {
                    continue;
                }
                for (auto* o : m_cells[CellIndex(r, c)].objects)
                {
                    if (o != exclude)
                    {
                        result.push_back(o);
                    }
                }
            }
        }
        return result;
    }

    void CollectAdjacentSet(int row, int col, GameObject* exclude,
                            std::unordered_set<GameObject*>& out) const
    {
        for (int dr = -1; dr <= 1; ++dr)
        {
            for (int dc = -1; dc <= 1; ++dc)
            {
                int r = row + dr;
                int c = col + dc;
                if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
                {
                    continue;
                }
                for (auto* o : m_cells[CellIndex(r, c)].objects)
                {
                    if (o != exclude)
                    {
                        out.insert(o);
                    }
                }
            }
        }
    }

private:
    float m_minX;
    float m_minZ;
    float m_cellSize;
    float m_hysteresisBuffer;

    int m_rows{0};
    int m_cols{0};

    std::vector<Cell> m_cells;
    std::unordered_map<GameObject*, std::pair<int, int>> m_objectCellMap;
};
