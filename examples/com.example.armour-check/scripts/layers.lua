-- How the game stacks worn apparel on one body part (equipment refresh 0xECA40):
-- the part keeps its outermost piece, plus one merged row for everything under it. Pieces are given in wear
-- order, innermost first. The coverage bonus one apparel class gets is left out, as in ExaniMate's preview.
local layers = {}

local KINDS = {"cut", "crush", "pierce"}

-- A row's weight on a hit: full when the roll lands inside its coverage, partial above 50% coverage.
local function hit_weight(chance)
    if chance <= 0.5 then return chance end
    return chance + (1 - chance) * (chance - 0.5) * 1.75
end

-- rows(pieces, part) -> outer, under (under is nil with one piece). Each row is
-- {chance, impact, cut, crush, pierce}; pieces are items.stats(...).armour tables.
function layers.rows(pieces, part)
    local outer, under
    for _, armour in ipairs(pieces) do
        local coverage = armour.coverage[part]
        if coverage then
            if outer then
                local w1 = hit_weight(outer.chance)
                local w0 = under and hit_weight(under.chance) or 0
                local base = under or {impact = 0, cut = 0, crush = 0, pierce = 0}
                under = {chance = 1, impact = outer.impact * w1 + base.impact * w0}
                for _, kind in ipairs(KINDS) do
                    local p0 = base[kind] * w0
                    under[kind] = p0 + (1 - p0) * outer[kind] * w1
                end
            end
            outer = {chance = coverage / 100, impact = armour.impact, cut = armour.cut, crush = armour.crush, pierce = armour.pierce}
        end
    end
    return outer, under
end

-- The weight a row counts with, averaged over the game's one random roll per hit.
function layers.expected_weight(row)
    local partial = row.chance > 0.5 and (row.chance - 0.5) * 1.75 or 0
    return row.chance + (1 - row.chance) * partial
end

-- Average protection on the part for each damage kind: rows combine as P + (1 - P) * p * w, impact adds.
-- (The roll is shared by both rows, so this average is a preview, not the game's exact distribution.)
function layers.expected(outer, under)
    local result = {impact = 0, cut = 0, crush = 0, pierce = 0}
    for _, row in ipairs({outer, under}) do
        local w = layers.expected_weight(row)
        result.impact = result.impact + row.impact * w
        for _, kind in ipairs(KINDS) do
            result[kind] = result[kind] + (1 - result[kind]) * row[kind] * w
        end
    end
    return result
end

return layers
