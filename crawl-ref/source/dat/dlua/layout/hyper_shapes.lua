------------------------------------------------------------------------------
-- hyper_shapes.lua:
--
-- Creates shapes for layout paint arrays

require("dlua/layout/procedural.lua")

hyper.shapes = {}
hyper.floors = {}

---------------------------------------------------------------------
-- Size callbacks
--
-- To be used in room generators to produce different types of sizings

-- TODO: There are two important factors - the distribution (i.e. do we tend towards
--       average, the lower end, or an even distribution between the min/max?) -
--       and the type of shape produced - square, any shape, narrow, near-square, etc.
--       Could think about separating these two factors for easier control
--       without having to repeat so much of the same logic each time.

-- Crudely produces a size tending towards the lower end of the min/max settings
function hyper.rooms.size_default(generator,options)
  -- Use min sizes from available config
  local min_size_x, max_size_x, min_size_y, max_size_y = options.min_room_size,options.max_room_size,options.min_room_size,options.max_room_size
  if generator.min_size ~= nil then min_size_x,min_size_y = generator.min_size,generator.min_size end
  if generator.max_size ~= nil then max_size_x,max_size_y = generator.max_size,generator.max_size end
  if generator.min_size_x ~= nil then min_size_x = generator.min_size_x end
  if generator.max_size_x ~= nil then max_size_x = generator.max_size_x end
  if generator.min_size_y ~= nil then min_size_y = generator.min_size_y end
  if generator.max_size_y ~= nil then max_size_y = generator.max_size_y end

  local diffx,diffy = max_size_x - min_size_x + 1, max_size_y - min_size_y + 1
  return { x = min_size_x + crawl.random2(crawl.random2(diffx)), y = min_size_y + crawl.random2(crawl.random2(diffy)) }
end

-- Produces only square sizes. Even distribution.
function hyper.rooms.size_square(chosen,options)
  local min_size, max_size = options.min_room_size,options.max_room_size
  if chosen.min_size ~= nil then min_size = chosen.min_size end
  if chosen.max_size ~= nil then max_size = chosen.max_size end

  local size = crawl.random_range(min_size,max_size)
  return { x = size, y = size }
end

function hyper.rooms.size_square_lower(chosen,options)
  -- Use the default distribution
  local size = hyper.rooms.size_default(chosen,options)
  -- Just take x sizes
  return { x = size.x, y = size.x }
end

---------------------------------------------------------------------
-- Util functions

-- Merge a set of properties onto every item in a paint array
function hyper.shapes.merge_props(paint, props)
  if props == nil then return paint end
  for i,p in ipairs(paint) do
    for k,prop in pairs(props) do
      p[k] = prop
    end
  end
end

---------------------------------------------------------------------
-- Draw functions
--
-- These take input parameters and return paint arrays (which should
-- be combined with util.append when using multiple draw functions).
-- These will be used by other room functions.
--
-- TODO: For any of these we should support an additional parameter
--       for specifying arbitrary properties of the returned paint, *or*
--       some sort of filter util which does the same on a whole batch afterward.
function hyper.shapes.draw_box(corner1, corner2)

  return {
    { type = "floor", corner1 = corner1, corner2 = corner2, open = true },
  }

end

---------------------------------------------------------------------
-- Room functions
--
-- These generate code rooms and are used by "code" generators in
-- the options tables.

-- TODO: These functions are 'rooms' not 'shapes'. They probably can't go in hyper_rooms.lua but where...?
-- A square room made of floor
function hyper.rooms.floor_vault(room,options)
  return {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 } },
  }
end

-- A square room made of wall
function hyper.rooms.wall_vault(room,options)
  return {
    { type = "wall", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 } },
  }
end

function hyper.rooms.bubble_vault(room,options,gen)
  return {
    { type = "floor", corner1 = { x = 0, y = 0}, corner2 = { x = room.size.x - 1, y = room.size.y - 1 }, shape = "ellipse" },
  }
end

-- A really heavily distorted small room. Tends to have connectivity pockets.
-- TODO: Some sort of conectivity analyser that works out where to put doors
function hyper.rooms.cavern_vault(room,options,gen)

  local fborder = procedural.border{ x1 = -1, y1 = -1, x2 = room.size.x, y2 = room.size.y, padding = 2 }
  local fwall = procedural.worley_diff { scale = 0.5 }

  return {
    { type = "proc", callback = function(x,y,mx,my)
      local combi = (1-fborder(x,y)) * (0.1+fwall(x,y))
      return combi < 0.1 and "space" or "floor"
    end }
  }
end

function hyper.rooms.cave_vault(room,options,gen)
  return hyper.floors.cave_paint({x = 0, y = 0},{ x = room.size.x-1,y = room.size.y-1}, {
    feature = "floor",
    wall = "wall",
    outer_break = 2, -- 1
    wall_break = 0.9, -- 0.8
    noise_scale = 4,
    padding = 4 -- 2
    })
end

function hyper.rooms.cave_walls(room,options,gen)
  return hyper.floors.cave_paint({x = 0, y = 0},{ x = room.size.x-1,y = room.size.y-1})
end

-- A nice crinkly cavernous border usually with a small number of decent-sized internal structures
function hyper.floors.cavern_walls(room,options,gen)
  local fborder = procedural.border{ x1 = 0, y1 = 0, x2 = room.size.x-1, y2 = room.size.y-1, padding = 10 }
  local fwall = procedural.worley_diff { scale = 0.25 }
  local wdiff = util.random_range_real(0.7,0.9)
  return {
    { type = "proc", c1 = { x = 0, y = 0 }, c2 = { x = room.size.x-1, y = room.size.y-1 }, callback = function(x,y,mx,my)
      local combi = fwall(x,y)+fborder(x,y)
      return combi >= wdiff and "space" or "floor"
    end }
  }
end

function hyper.floors.cave_paint(corner1,corner2,params)
  local defaults = {
      feature = "floor",
      wall = nil,
      outer_break = 1.0,
      wall_break = 0.8,
      noise_scale = 2
    }
  params = params ~= nil and hyper.merge_options(defaults,params) or defaults
  local x1,y1,x2,y2 = corner1.x,corner1.y,corner2.x,corner2.y
  local padding = params.padding or math.max(1,crawl.div_rand_round(math.min(x2-x1, y2-y1),8))
  local fborder = procedural.border{ x1 = x1-1, y1 = y1-1, x2 = x2+1, y2 = y2+1, padding = padding, additive = true }
  local fwall = procedural.simplex3d { scale = params.noise_scale }

  return {
    { type = "proc", corner1 = corner1, corner2 = corner2, callback = function(x,y,mx,my)
      -- This was another one that made great patterns but loads of dead zones: (with scale = 2 )
      -- local combi = (1-fborder(x,y)) * (0.1+fwall(x,y))
      -- Standard algorithm looked pretty good but some internal junk:
      -- local combi = fborder(x,y) + fwall(x,y)
      local combi = fborder(x,y)*2 + fwall(x,y)
      -- There was an error in this where combi < 0.5, it produced all kinds of wacky looking things but was awesome (scale = 4)
      if combi > params.outer_break then return nil
      elseif params.wall and combi > params.wall_break then return params.wall, { carvable = true } end
      return params.feature
    end }
  }
end

-- Draws a floor plan based on omnigrid
function hyper.floors.floor_plan_omnigrid(room,options,gen)

  local paint = hyper.layout.omnigrid_primitive{
    guaranteed_divides = 0,
    subdivide_initial_chance = 80,  -- 20% chance of a donut
    fill_chance = 100,
    corridor_width = 8,
    minimum_size = 12,
    jitter = true,
    jitter_min = -2,
    jitter_max = 2,
    outer_corridor = true,
    size = room.size,
    paint_func = crawl.one_chance_in(3) and hyper.floors.cave_paint or nil,
    floor_func = crawl.one_chance_in(2) and hyper.floors.cave_paint or nil
  }
  return paint
end

function hyper.floors.floor_plan_finegrid(room,options,gen)
  local corridor_width = crawl.random_range(1,3)
  local jitters = {
    { weight = 20, jitter = false },
    { weight = 10, jitter = true, jitter_min = -3, jitter_max = 0 },
    -- With high jitter, it turns into really chaotic random architecture
    { weight = 5, jitter = true, jitter_min = -2, jitter_max = 1 },
    { weight = 5, jitter = true, jitter_min = -2, jitter_max = 2 }
  }
  local chosen = util.random_weighted_from("weight",jitters)

  local paint = hyper.layout.omnigrid_primitive{
    guaranteed_divides = 6,
    subdivide_initial_chance = 100,  -- 20% chance of a donut
    subdivide_level_multiplier = 0.98,
    corridor_width = corridor_width,
    fill_chance = 80,
    minimum_size = 6,
    jitter = chosen.jitter,
    jitter_min = chosen.jitter_min,
    jitter_max = chosen.jitter_max,
    size = room.size,
    paint_func = crawl.one_chance_in(3) and hyper.floors.cave_paint or nil,
    floor_func = crawl.one_chance_in(3) and hyper.floors.cave_paint or nil
  }
  return paint
end

function hyper.floors.floor_plan_microgrid(room,options,gen)
  local paint = hyper.layout.omnigrid_primitive{
    guaranteed_divides = 5,
    subdivide_initial_chance = 100,  -- 20% chance of a donut
    subdivide_level_multiplier = util.random_range_real(0.9,0.999),
    corridor_width = 1,
    fill_chance = crawl.random_range(50,90),
    minimum_size = 2,
    jitter = false,
    size = room.size,
    paint_func = crawl.one_chance_in(5) and hyper.floors.cave_paint or nil,
    floor_func = crawl.one_chance_in(4) and hyper.floors.cave_paint or nil
  }
  return paint
end

-- Layer any two of the other grid plans over each other
function hyper.floors.floor_plan_combigrid(room,options,gen)
  local which = { hyper.floors.floor_plan_omnigrid, hyper.floors.floor_plan_finegrid, hyper.floors.microgrid }
  local whichnum = crawl.random2(#which)
  table.remove(which,whichnum+1)
  local paint = {}
  for i,plan in which do
    util.append(paint,plan(room,options,gen))
  end
  return paint
end

function hyper.rooms.junction_vault(room,options,gen)

  local floor = hyper.rooms.floor_vault(room,options,gen)

  local max_corridor = math.min(gen.max_corridor, math.floor(math.min(room.size.x,room.size.y)/2))
  local corridor_width = crawl.random_range(gen.min_corridor,max_corridor)

  -- Use cunning grid wrapping to generate all kinds of corners and intersections
  local corner1 = { x = crawl.random_range(corridor_width,room.size.x), y = crawl.random_range(corridor_width,room.size.y) }
  table.insert(floor, { type = "space", corner1 = corner1, corner2 = { x = corner1.x + room.size.x - 1 - corridor_width, y = corner1.y + room.size.y - 1 - corridor_width }, wrap = true })
  return floor
end

---------------------------------------------------------------------
-- Floor functions
--
-- Describe initial floor layouts to paint the entire grid with.
--
-- TODO: Basically completely redundant now



function hyper.floors.full_size_room()
  local gxm, gym = dgn.max_bounds()

  local paint = {
    { type = "floor", corner1 = { x = 1, y = 1 }, corner2 = { x = gxm-2, y = gym-2 } }
  }
  return paint
end

function hyper.floors.three_quarters_size_room()
  local gxm, gym = dgn.max_bounds()

  local paint = {
    { type = "floor", corner1 = { x = 11, y = 11 }, corner2 = { x = gxm-12, y = gym-12 } }
  }
  return paint
end

function hyper.floors.small_central_room()
  local gxm, gym = dgn.max_bounds()
  -- Put a single empty room somewhere roughly central. All rooms will be built off from each other following this
  local x1 = crawl.random_range(30, gxm-70)
  local y1 = crawl.random_range(30, gym-70)

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + crawl.random_range(4,6), y = y1 + crawl.random_range(4,6) } }
  }
  return paint
end

function hyper.floors.small_edge_room()
  local gxm, gym = dgn.max_bounds()
  local normal = hyper.normals[crawl.random2(4)+1]
  local size = { x = crawl.random_range(4,8), y = crawl.random_range(4,8) }

  local x1 = math.floor((normal.x + 1)*(gxm - 10 - size.x)/2 + 5)
  local y1 = math.floor((normal.y + 1)*(gym - 10 - size.y)/2 + 5)

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + size.x - 1, y = y1 + size.y - 1 } }
  }
  return paint
end
