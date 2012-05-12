// mutation definitions:
// first  number  = probability (0 means it doesn't appear naturally
//                  unless you have some level of it already)
// second number  = maximum levels
// first  boolean = is mutation mostly bad?
// second boolean = is mutation physical, i.e. external only?
// third  boolean = is mutation suppressed when shapechanged?
// first  strings = what to show in 'A'
// second strings = message given when gaining the mutation
// third  strings = message given when losing the mutation
// fourth string  = wizard-mode name of mutation

#ifndef MUTATION_DATA_H
#define MUTATION_DATA_H

{ MUT_TOUGH_SKIN,                0,  3, false,  true, true,
  M_("tough skin"),

  {N_("You have tough skin (AC +1)."),
   N_("You have very tough skin (AC +2)."),
   N_("You have extremely tough skin (AC +3).")},

  {N_("Your skin toughens."),
   N_("Your skin toughens."),
   N_("Your skin toughens.")},

  {N_("Your skin feels delicate."),
   N_("Your skin feels delicate."),
   N_("Your skin feels delicate.")},

  "tough skin"
},

{ MUT_STRONG,                     8, 14, false,  true, false,
  NULL,

  {N_("Your muscles are strong (Str +"), "", ""},
  {"", "", ""},
  {"", "", ""},

  "strong"
},

{ MUT_CLEVER,                     8, 14, false,  true, false,
  NULL,

  {N_("Your mind is acute (Int +"), "", ""},
  {"", "", ""},
  {"", "", ""},

  "clever"
},

{ MUT_AGILE,                      8, 14, false,  true, false,
  NULL,

  {N_("You are agile (Dex +"), "", ""},
  {"", "", ""},
  {"", "", ""},

  "agile"
},

{ MUT_POISON_RESISTANCE,          4,  1, false, false, true,
  M_("poison resistance"),

  {N_("Your system is resistant to poisons."), "", ""},
  {N_("You feel healthy."), "",  ""},
  {N_("You feel a little less healthy."), "", ""},

  "poison resistance"
},

{ MUT_CARNIVOROUS,                5,  3, false, false, false,
  M_("carnivore"),

  {N_("Your digestive system is specialised to digest meat."),
   N_("Your digestive system is highly specialised to digest meat."),
   N_("You are carnivorous and can eat meat at any time.")},

  {N_("You hunger for flesh."),
   N_("You hunger for flesh."),
   N_("You hunger for flesh.")},

  {N_("You feel able to eat a more balanced diet."),
   N_("You feel able to eat a more balanced diet."),
   N_("You feel able to eat a more balanced diet.")},

  "carnivorous"
},

{ MUT_HERBIVOROUS,                5,  3,  true, false, false,
  M_("herbivore"),

  {N_("You digest meat inefficiently."),
   N_("You digest meat very inefficiently."),
   N_("You are a herbivore.")},

  {N_("You hunger for vegetation."),
   N_("You hunger for vegetation."),
   N_("You hunger for vegetation.")},

  {N_("You feel able to eat a more balanced diet."),
   N_("You feel able to eat a more balanced diet."),
   N_("You feel able to eat a more balanced diet.")},

  "herbivorous"
},

{ MUT_HEAT_RESISTANCE,            4,  3, false, false, true,
  M_("fire resistance"),

  {N_("Your flesh is heat resistant."),
   N_("Your flesh is very heat resistant."),
   N_("Your flesh is almost immune to the effects of heat.")},

  {N_("You feel a sudden chill."),
   N_("You feel a sudden chill."),
   N_("You feel a sudden chill.")},

  {N_("You feel hot for a moment."),
   N_("You feel hot for a moment."),
   N_("You feel hot for a moment.")},

  "heat resistance"
},

{ MUT_COLD_RESISTANCE,            4,  3, false, false, true,
  M_("cold resistance"),

  {N_("Your flesh is cold resistant."),
   N_("Your flesh is very cold resistant."),
   N_("Your flesh is almost immune to the effects of cold.")},

  {N_("You feel hot for a moment."),
   N_("You feel hot for a moment."),
   N_("You feel hot for a moment.")},

  {N_("You feel a sudden chill."),
   N_("You feel a sudden chill."),
   N_("You feel a sudden chill.")},

  "cold resistance"
},

{ MUT_DEMONIC_GUARDIAN,            0,  3, false, false, false,
  M_("demonic guardian"),

  {N_("A weak demonic guardian rushes to your aid."),
   N_("A demonic guardian rushes to your aid."),
   N_("A powerful demonic guardian rushes to your aid.")},

  {N_("You feel the presence of a demonic guardian."),
   N_("Your guardian grows in power."),
   N_("Your guardian grows in power.")},

  {N_("Your demonic guardian is gone."),
   N_("Your demonic guardian is weakened."),
   N_("Your demonic guardian is weakened.")},

  "demonic guardian"
},

{ MUT_SHOCK_RESISTANCE,           2,  1, false, false, true,
  M_("electricity resistance"),

  {N_("You are resistant to electric shocks."), "", ""},
  {N_("You feel insulated."), "", ""},
  {N_("You feel conductive."), "", ""},

  "shock resistance"
},

{ MUT_REGENERATION,               3,  3, false, false, false,
  M_("regeneration"),

  {N_("Your natural rate of healing is unusually fast."),
   N_("You heal very quickly."),
   N_("You regenerate.")},

  {N_("You begin to heal more quickly."),
   N_("You begin to heal more quickly."),
   N_("You begin to regenerate.")},

  {N_("Your rate of healing slows."),
   N_("Your rate of healing slows."),
   N_("Your rate of healing slows.")},

  "regeneration"
},

{ MUT_SLOW_HEALING,               3,  3,  true, false, false,
  M_("slow healing"),

  {N_("You heal slowly."),
   N_("You heal very slowly."),
   N_("You do not heal naturally.")},

  {N_("You begin to heal more slowly."),
   N_("You begin to heal more slowly."),
   N_("You stop healing.")},

  {N_("Your rate of healing increases."),
   N_("Your rate of healing increases."),
   N_("Your rate of healing increases.")},

  "slow healing"
},

{ MUT_FAST_METABOLISM,           10,  3,  true, false, false,
  M_("fast metabolism"),

  {N_("You have a fast metabolism."),
   N_("You have a very fast metabolism."),
   N_("Your metabolism is lightning-fast.")},

  {N_("You feel a little hungry."),
   N_("You feel a little hungry."),
   N_("You feel a little hungry.")},

  {N_("Your metabolism slows."),
   N_("Your metabolism slows."),
   N_("Your metabolism slows.")},

  "fast metabolism"
},

{ MUT_SLOW_METABOLISM,            7,  3, false, false, false,
  M_("slow metabolism"),

  {N_("You have a slow metabolism."),
   N_("You have a slow metabolism."),
   N_("You need consume almost no food.")},

  {N_("Your metabolism slows."),
   N_("Your metabolism slows."),
   N_("Your metabolism slows.")},

  {N_("You feel a little hungry."),
   N_("You feel a little hungry."),
   N_("You feel a little hungry.")},

  "slow metabolism"
},

{ MUT_WEAK,                      10, 14,  true,  true, false,
  NULL,
  {N_("You are weak (Str -"), "", ""},
  {"", "", ""},
  {"", "", ""},
  "weak"
},

{ MUT_DOPEY,                     10, 14,  true,  true, false,
  NULL,
  {N_("You are dopey (Int -"), "", ""},
  {"", "", ""},
  {"", "", ""},
  "dopey",
},

{ MUT_CLUMSY,                    10, 14,  true,  true, false,
  NULL,
  {N_("You are clumsy (Dex -"), "", ""},
  {"", "", ""},
  {"", "", ""},
  "clumsy"
},

{ MUT_TELEPORT_CONTROL,           2,  1, false, false, false,
  M_("teleport control"),

  {N_("You can control translocations."), "", ""},
  {N_("You feel controlled."), "", ""},
  {N_("You feel random."), "", ""},

  "teleport control"
},

{ MUT_TELEPORT,                   3,  3,  true, false, false,
  M_("teleportitis"),

  {N_("Space occasionally distorts in your vicinity."),
   N_("Space sometimes distorts in your vicinity."),
   N_("Space frequently distorts in your vicinity.")},

  {N_("You feel weirdly uncertain."),
   N_("You feel even more weirdly uncertain."),
   N_("You feel even more weirdly uncertain.")},

  {N_("You feel stable."),
   N_("You feel stable."),
   N_("You feel stable.")},

  "teleport"
},

{ MUT_MAGIC_RESISTANCE,           5,  3, false, false, false,
  M_("magic resistance"),

  {N_("You are resistant to hostile enchantments."),
   N_("You are highly resistant to hostile enchantments."),
   N_("You are extremely resistant to the effects of hostile enchantments.")},

  {N_("You feel resistant to hostile enchantments."),
   N_("You feel more resistant to hostile enchantments."),
   N_("You feel almost impervious to the effects of hostile enchantments.")},

  {N_("You feel less resistant to hostile enchantments."),
   N_("You feel less resistant to hostile enchantments."),
   N_("You feel vulnerable to magic hostile enchantments.")},

  "magic resistance"
},

{ MUT_FAST,                       1,  3, false, false, true,
  M_("speed"),

  {N_("You cover ground quickly."),
   N_("You cover ground very quickly."),
   N_("You cover ground extremely quickly.")},

  {N_("You feel quick."),
   N_("You feel quick."),
   N_("You feel quick.")},

  {N_("You feel sluggish."),
   N_("You feel sluggish."),
   N_("You feel sluggish.")},

  "fast"
},

{ MUT_SLOW,                       3,  3, true, false, true,
  M_("slowness"),

  {N_("You cover ground slowly."),
   N_("You cover ground very slowly."),
   N_("You cover ground extremely slowly.")},

  {N_("You feel sluggish."),
   N_("You feel sluggish."),
   N_("You feel sluggish.")},

  {N_("You feel quick."),
   N_("You feel quick."),
   N_("You feel quick.")},

  "slow"
},

{ MUT_ACUTE_VISION,               2,  1, false, false, false,
  M_("see invisible"),

  {N_("You have supernaturally acute eyesight."), "", ""},

  {N_("Your vision sharpens."),
   N_("Your vision sharpens."),
   N_("Your vision sharpens.")},

  {N_("Your vision seems duller."),
   N_("Your vision seems duller."),
   N_("Your vision seems duller.")},

  "acute vision"
},

{ MUT_DEFORMED,                   8,  1,  true,  true, true,
  M_("deformed body"),

  {N_("Armour fits poorly on your strangely shaped body."), "", ""},
  {N_("Your body twists and deforms."), "", ""},
  {N_("Your body's shape seems more normal."), "", ""},

  "deformed"
},

{ MUT_SPIT_POISON,                8,  3, false, false, false,
  M_("spit poison"),

  {N_("You can spit poison."),
   N_("You can spit moderately strong poison."),
   N_("You can spit strong poison.")},

  {N_("There is a nasty taste in your mouth for a moment."),
   N_("There is a nasty taste in your mouth for a moment."),
   N_("There is a nasty taste in your mouth for a moment.")},

  {N_("You feel an ache in your throat."),
   N_("You feel an ache in your throat."),
   N_("You feel an ache in your throat.")},

  "spit poison"
},

{ MUT_BREATHE_FLAMES,             4,  3, false, false, false,
  M_("breathe flames"),

  {N_("You can breathe flames."),
   N_("You can breathe fire."),
   N_("You can breathe blasts of fire.")},

  {N_("Your throat feels hot."),
   N_("Your throat feels hot."),
   N_("Your throat feels hot.")},

  {N_("A chill runs up and down your throat."),
   N_("A chill runs up and down your throat."),
   N_("A chill runs up and down your throat.")},

  "breathe flames"
},

{ MUT_BLINK,                      3,  3, false, false, false,
  M_("blink"),

  {N_("You can translocate small distances at will."),
   N_("You are good at translocating small distances at will."),
   N_("You can easily translocate small distances at will.")},

  {N_("You feel jittery."),
   N_("You feel more jittery."),
   N_("You feel even more jittery.")},

  {N_("You feel a little less jittery."),
   N_("You feel less jittery."),
   N_("You feel less jittery.")},

  "blink"
},

{ MUT_STRONG_STIFF,              10,  3, false,  true, false,
  NULL,

  {N_("Your muscles are strong, but stiff (Str +1, Dex -1)."),
   N_("Your muscles are very strong, but stiff (Str +2, Dex -2)."),
   N_("Your muscles are extremely strong, but stiff (Str +3, Dex -3).")},

  {N_("Your muscles feel sore."),
   N_("Your muscles feel sore."),
   N_("Your muscles feel sore.")},

  {N_("Your muscles feel loose."),
   N_("Your muscles feel loose."),
   N_("Your muscles feel loose.")},

  "strong stiff"
},

{ MUT_FLEXIBLE_WEAK,             10,  3, false,  true, false,
  NULL,

  {N_("Your muscles are flexible, but weak (Str -1, Dex +1)."),
   N_("Your muscles are very flexible, but weak (Str -2, Dex +2)."),
   N_("Your muscles are extremely flexible, but weak (Str -3, Dex +3).")},

  {N_("Your muscles feel loose."),
   N_("Your muscles feel loose."),
   N_("Your muscles feel loose.")},

  {N_("Your muscles feel sore."),
   N_("Your muscles feel sore."),
   N_("Your muscles feel sore.")},

  "flexible weak"
},

{ MUT_SCREAM,                     6,  3,  true, false, false,
  M_("screaming"),

  {N_("You occasionally shout uncontrollably."),
   N_("You sometimes yell uncontrollably."),
   N_("You frequently scream uncontrollably.")},

  {N_("You feel the urge to shout."),
   N_("You feel a strong urge to yell."),
   N_("You feel a strong urge to scream.")},

  {N_("Your urge to shout disappears."),
   N_("Your urge to yell lessens."),
   N_("Your urge to scream lessens.")},

  "scream"
},

{ MUT_CLARITY,                    6,  1, false, false, false,
  M_("clarity"),

  {N_("You possess an exceptional clarity of mind."), "", ""},
  {N_("Your thoughts seem clearer."), "", ""},
  {N_("Your thinking seems confused."), "", ""},

  "clarity"
},

{ MUT_BERSERK,                    7,  3,  true, false, false,
  M_("berserk"),

  {N_("You tend to lose your temper in combat."),
   N_("You often lose your temper in combat."),
   N_("You have an uncontrollable temper.")},

  {N_("You feel a little pissed off."),
   N_("You feel angry."),
   N_("You feel extremely angry at everything!")},

  {N_("You feel a little more calm."),
   N_("You feel a little less angry."),
   N_("You feel a little less angry.")},

  "berserk"
},

{ MUT_DETERIORATION,             10,  3,  true, false, false,
  M_("deterioration"),

  {N_("Your body is slowly deteriorating."),
   N_("Your body is deteriorating."),
   N_("Your body is rapidly deteriorating.")},

  {N_("You feel yourself wasting away."),
   N_("You feel yourself wasting away."),
   N_("You feel your body start to fall apart.")},

  {N_("You feel healthier."),
   N_("You feel a little healthier."),
   N_("You feel a little healthier.")},

  "deterioration"
},

{ MUT_BLURRY_VISION,             10,  3,  true, false, false,
  M_("blurry vision"),

  {N_("Your vision is a little blurry."),
   N_("Your vision is quite blurry."),
   N_("Your vision is extremely blurry.")},

  {N_("Your vision blurs."),
   N_("Your vision blurs."),
   N_("Your vision blurs.")},

  {N_("Your vision sharpens."),
   N_("Your vision sharpens a little."),
   N_("Your vision sharpens a little.")},

  "blurry vision"
},

{ MUT_MUTATION_RESISTANCE,        4,  3, false, false, false,
  M_("mutation resistance"),

  {N_("You are somewhat resistant to further mutation."),
   N_("You are somewhat resistant to both further mutation and mutation removal."),
   N_("Your current mutations are irrevocably fixed, and you can mutate no more.")},

  {N_("You feel genetically stable."),
   N_("You feel genetically stable."),
   N_("You feel genetically immutable.")},

  {N_("You feel genetically unstable."),
   N_("You feel genetically unstable."),
   N_("You feel genetically unstable.")},

  "mutation resistance"
},

{ MUT_EVOLUTION,                  4,  2, false, false, false,
  "evolution",

  {"You evolve.",
   "You rapidly evolve.",
   ""},

  {"You feel nature experimenting on you. Don't worry, failures die fast.",
   "Your genes go into a fast flux.",
   ""},

  {"Your wild genetic ride slows down.",
   "You feel genetically stable.",
   ""},

  "evolution"
},

{ MUT_FRAIL,                     10,  3,  true,  true, false,
  NULL,

  {N_("You are frail (-10% HP)."),
   N_("You are very frail (-20% HP)."),
   N_("You are extremely frail (-30% HP).")},

  {N_("You feel frail."),
   N_("You feel frail."),
   N_("You feel frail.")},

  {N_("You feel robust."),
   N_("You feel robust."),
   N_("You feel robust.")},

  "frail"
},

{ MUT_ROBUST,                     5,  3, false,  true, false,
  NULL,

  {N_("You are robust (+10% HP)."),
   N_("You are very robust (+20% HP)."),
   N_("You are extremely robust (+30% HP).")},

  {N_("You feel robust."),
   N_("You feel robust."),
   N_("You feel robust.")},

  {N_("You feel frail."),
   N_("You feel frail."),
   N_("You feel frail.")},

  "robust"
},

{ MUT_UNBREATHING,                0,  1, false, false, true,
  M_("unbreathing"),

  {N_("You can survive without breathing."), "", ""},
  {N_("You feel breathless."), "", ""},
  {"", "", ""},

  "unbreathing"
},

{ MUT_TORMENT_RESISTANCE,         0,  1, false, false, false,
  M_("torment resistance"),

  {N_("You are immune to unholy pain and torment."), "", ""},
  {N_("You feel a strange anaesthesia."), "", ""},
  {"", "", ""},

  "torment resistance"
},

{ MUT_NEGATIVE_ENERGY_RESISTANCE, 0,  3, false, false, true,
  M_("life protection"),

  {N_("You resist negative energy."),
   N_("You are quite resistant to negative energy."),
   N_("You are immune to negative energy.")},

  {N_("You feel negative."),
   N_("You feel negative."),
   N_("You feel negative.")},

  {"", "", ""},

  "negative energy resistance"
},

{ MUT_HURL_HELLFIRE,              0,  1, false, false, false,
  M_("hurl hellfire"),

  {N_("You can hurl blasts of hellfire."), "", ""},
  {N_("You smell fire and brimstone."), "", ""},
  {"", "", ""},

  "hurl hellfire"
},

// body-slot facets
{ MUT_HORNS,                      7,  3, false,  true, true,
  M_("horns"),

  {N_("You have a pair of small horns on your head."),
   N_("You have a pair of horns on your head."),
   N_("You have a pair of large horns on your head.")},

  {N_("A pair of horns grows on your head!"),
   N_("The horns on your head grow some more."),
   N_("The horns on your head grow some more.")},

  {N_("The horns on your head shrink away."),
   N_("The horns on your head shrink a bit."),
   N_("The horns on your head shrink a bit.")},

  "horns"
},

{ MUT_BEAK,                       1,  1, false,  true, true,
  M_("beak"),

  {N_("You have a beak for a mouth."), "", ""},
  {N_("Your mouth lengthens and hardens into a beak!"), "", ""},
  {N_("Your beak shortens and softens into a mouth."), "", ""},

  "beak"
},

{ MUT_CLAWS,                      2,  3, false,  true, true,
  M_("claws"),

  {N_("You have sharp fingernails."),
   N_("You have very sharp fingernails."),
   N_("You have claws for hands.")},

  {N_("Your fingernails lengthen."),
   N_("Your fingernails sharpen."),
   N_("Your hands twist into claws.")},

  {N_("Your fingernails shrink to normal size."),
   N_("Your fingernails look duller."),
   N_("Your hands feel fleshier.")},

  "claws"
},

{ MUT_FANGS,                      1,  3, false,  true, true,
  M_("fangs"),

  {N_("You have very sharp teeth."),
   N_("You have extremely sharp teeth."),
   N_("You have razor-sharp teeth.")},

  {N_("Your teeth lengthen and sharpen."),
   N_("Your teeth lengthen and sharpen some more."),
   N_("Your teeth are very long and razor-sharp.")},

  {N_("Your teeth shrink to normal size."),
   N_("Your teeth shrink and become duller."),
   N_("Your teeth shrink and become duller.")},

  "fangs"
},

{ MUT_HOOVES,                     5,  3, false,  true, true,
  M_("hooves"),

  {N_("You have large cloven feet."),
   N_("You have hoof-like feet."),
   N_("You have hooves in place of feet.")},

  {N_("Your feet thicken and deform."),
   N_("Your feet thicken and deform."),
   N_("Your feet have mutated into hooves.")},

  {N_("Your hooves expand and flesh out into feet!"),
   N_("Your hooves look more like feet."),
   N_("Your hooves look more like feet.")},

  "hooves"
},

{ MUT_ANTENNAE,                   4,  3, false,  true, true,
  M_("antennae"),

  {N_("You have a pair of small antennae on your head."),
   N_("You have a pair of antennae on your head."),
   N_("You have a pair of large antennae on your head (SInv).")},

  {N_("A pair of antennae grows on your head!"),
   N_("The antennae on your head grow some more."),
   N_("The antennae on your head grow some more.")},

  {N_("The antennae on your head shrink away."),
   N_("The antennae on your head shrink a bit."),
   N_("The antennae on your head shrink a bit.")},

  "antennae"
},

{ MUT_TALONS,                     5,  3, false,  true, true,
  M_("talons"),

  {N_("You have sharp toenails."),
   N_("You have razor-sharp toenails."),
   N_("You have claws for feet.")},

  {N_("Your toenails lengthen and sharpen."),
   N_("Your toenails lengthen and sharpen."),
   N_("Your feet stretch into talons.")},

  {N_("Your talons dull and shrink into feet."),
   N_("Your talons look more like feet."),
   N_("Your talons look more like feet.")},

  "talons"
},

// Octopode only
{ MUT_TENTACLE_SPIKE,            0,  3, false,  true, true,
  "spike",

  {"One of your tentacles bears a spike.",
   "One of your tentacles bears a nasty spike.",
   "One of your tentacles bears a large vicious spike."},

  {"One of your lower tentacles grows a sharp spike.",
   "Your tentacle spike grows bigger.",
   "Your tentacle spike grows even bigger."},

  {"Your tentacle spike disappears.",
   "Your tentacle spike becomes smaller.",
   "Your tentacle spike recedes somewhat."},

  "tentacle spike"
},

// Naga only
{ MUT_BREATHE_POISON,             0,  1, false, false, true,
  M_("breathe poison"),

  {N_("You can exhale a cloud of poison."), "", ""},
  {N_("You taste something nasty."), "", ""},
  {N_("Your breath is less nasty."), "", ""},

  "breathe poison"
},

// Naga and Draconian only
{ MUT_STINGER,                    0,  3, false,  true, true,
  M_("stinger"),

  {N_("Your tail ends in a poisonous barb."),
   N_("Your tail ends in a sharp poisonous barb."),
   N_("Your tail ends in a wickedly sharp and poisonous barb.")},

  {N_("A poisonous barb forms on the end of your tail."),
   N_("The barb on your tail looks sharper."),
   N_("The barb on your tail looks very sharp.")},

  {N_("The barb on your tail disappears."),
   N_("The barb on your tail seems less sharp."),
   N_("The barb on your tail seems less sharp.")},

  "stinger"
},

// Draconian only
{ MUT_BIG_WINGS,                  0,  1, false,  true, true,
  M_("large and strong wings"),

  {N_("Your wings are large and strong."), "", ""},
  {N_("Your wings grow larger and stronger."), "", ""},
  {N_("Your wings shrivel and weaken."), "", ""},

  "big wings"
},

// species-dependent innate mutations
{ MUT_SAPROVOROUS,                0,  3, false, false, false,
  M_("saprovore"),

  {N_("You can tolerate rotten meat."),
   N_("You can eat rotten meat."),
   N_("You thrive on rotten meat.")},

  {N_("You hunger for rotting flesh."),
   N_("You hunger for rotting flesh."),
   N_("You hunger for rotting flesh.")},

  {"", "", ""},

  "saprovorous"
},

{ MUT_GOURMAND,                   0,  1, false, false, false,
  M_("gourmand"),

  {N_("You like to eat raw meat."), "", ""},
  {"", "", ""},
  {"", "", ""},

  "gourmand"
},

{ MUT_SHAGGY_FUR,                 2,  3, false,  true, true,
  NULL,

  {N_("You are covered in fur (AC +1)."),
   N_("You are covered in thick fur (AC +2)."),
   N_("Your thick and shaggy fur keeps you warm (AC +3, rC+).")},

  {N_("Fur sprouts all over your body."),
   N_("Your fur grows into a thick mane."),
   N_("Your thick fur grows shaggy and warm.")},

  {N_("You shed all your fur."),
   N_("Your thick fur recedes somewhat."),
   N_("Your shaggy fur recedes somewhat.")},

  "shaggy fur"
},

{ MUT_HIGH_MAGIC,                 2,  3, false, false, false,
  NULL,

  {N_("You have an increased reservoir of magic (+10% MP)."),
   N_("You have a considerably increased reservoir of magic (+20% MP)."),
   N_("You have a greatly increased reservoir of magic (+30% MP).")},

  {N_("You feel more energetic."),
   N_("You feel more energetic."),
   N_("You feel more energetic.")},

  {N_("You feel less energetic."),
   N_("You feel less energetic."),
   N_("You feel less energetic.")},

  "high mp"
},

{ MUT_LOW_MAGIC,                  9,  3,  true, false, false,
  NULL,

  {N_("Your magical capacity is low (-10% MP)."),
   N_("Your magical capacity is very low (-20% MP)."),
   N_("Your magical capacity is extremely low (-30% MP).")},

  {N_("You feel less energetic."),
   N_("You feel less energetic."),
   N_("You feel less energetic.")},

  {N_("You feel more energetic."),
   N_("You feel more energetic."),
   N_("You feel more energetic.")},

  "low mp"
},

{ MUT_WILD_MAGIC,                 6,   3,  false, false, false,
    M_("wild magic"),

    {N_("Your spells are a little harder to cast, but a little more powerful."),
     N_("Your spells are harder to cast, but more powerful."),
     N_("Your spells are much harder to cast, but much more powerful.")},

    {N_("You feel less in control of your magic."),
     N_("You feel less in control of your magic."),
     N_("You feel your magical power running wild!")},

    {N_("You regain control of your magic."),
     N_("You feel more in control of your magic."),
     N_("You feel more in control of your magic.")},

    "wild magic"
},

{ MUT_STOCHASTIC_TORMENT_RESISTANCE, 0,  1, false, false, false,
  M_("50% torment resistance"),

  {N_("You are somewhat able to resist unholy torments (1 in 2 success)."),"",""},
  {N_("You feel a strange anaesthesia."),"",""},
  {"","",""},

  "stochastic torment resistance"
},

{ MUT_PASSIVE_MAPPING,            3,  3, false, false, false,
  M_("sense surroundings"),

  {N_("You passively map a small area around you."),
   N_("You passively map the area around you."),
   N_("You passively map a large area around you.")},

  {N_("You feel a strange attunement to the structure of the dungeons."),
   N_("Your attunement to dungeon structure grows."),
   N_("Your attunement to dungeon structure grows further.")},

  {N_("You feel slightly disoriented."),
   N_("You feel slightly disoriented."),
   N_("You feel slightly disoriented.")},

  "passive mapping"
},

{ MUT_ICEMAIL,                    0,  1, false, false, false,
  NULL,

  {N_("A meltable icy envelope protects you from harm and freezing vapours (AC +"), "", ""},
  {N_("An icy envelope takes form around you."), "", ""},
  {"", "", ""},

  "icemail"
},

{ MUT_CONSERVE_SCROLLS,           0,  1, false, false, false,
  M_("conserve scrolls"),

  {N_("You are very good at protecting items from fire."), "", ""},
  {N_("You feel less concerned about heat."), "", ""},
  {"", "", ""},

  "conserve scrolls",
},

{ MUT_CONSERVE_POTIONS,           0,  1, false, false, false,
  M_("conserve potions"),

  {N_("You are very good at protecting items from cold."), "", ""},
  {N_("You feel less concerned about cold."), "", ""},
  {"", "", ""},
  "conserve potions",
},

{ MUT_PASSIVE_FREEZE,             0,  1, false, false, false,
  M_("passive freeze"),

  {N_("A frigid envelope surrounds you and freezes all who hurt you."), "", ""},
  {N_("Your skin feels very cold."), "", ""},
  {"", "", ""},

  "passive freeze",
},

{ MUT_NIGHTSTALKER,               0,  3, false, true, false,
  M_("nightstalker"),

  {N_("You are slightly more attuned to the shadows."),
   N_("You are significantly more attuned to the shadows."),
   N_("You are completely attuned to the shadows.")},

  {N_("You slip into the darkness of the dungeon."),
   N_("You slip further into the darkness."),
   N_("You are surrounded by darkness.")},

  {N_("Your affinity for the darkness vanishes."),
   N_("Your affinity for the darkness weakens."),
   N_("Your affinity for the darkness weakens.")},

  "nightstalker"
},

{ MUT_SPINY,                      0,  3, false, true, true,
  M_("spiny"),

  {N_("You are partially covered in sharp spines."),
   N_("You are mostly covered in sharp spines."),
   N_("You are completely covered in sharp spines.")},

  {N_("Sharp spines emerge from parts of your body."),
   N_("Sharp spines emerge from more of your body."),
   N_("Sharp spines emerge from your entire body.")},

  {N_("Your sharp spines disappear entirely."),
   N_("Your sharp spines retract somewhat."),
   N_("Your sharp spines retract somewhat.")},

  "spiny"
},

{ MUT_POWERED_BY_DEATH,           0,  3, false, false, false,
  M_("powered by death"),

  {N_("You can steal the life force of nearby defeated enemies."),
   N_("You can steal the life force of defeated enemies."),
   N_("You can steal the life force of all defeated enemies in sight.")},

  {N_("A wave of death washes over you."),
   N_("The wave of death grows in power."),
   N_("The wave of death grows in power.")},

  {N_("Your control of surrounding life forces is gone."),
   N_("Your control of surrounding life forces weakens."),
   N_("Your control of surrounding life forces weakens.")},

  "powered by death"
},

{ MUT_POWERED_BY_PAIN,            0,  3, false, false, false,
  M_("powered by pain"),

  {N_("You sometimes regain a little magical energy from taking damage."),
   N_("You sometimes regain magical energy from taking damage."),
   N_("You sometimes regain a lot of magical energy from taking damage.")},

  {N_("You feel energised by your suffering."),
   N_("You feel even more energised by your suffering."),
   N_("You feel completely energised by your suffering.")},

  {"", "", ""},

  "powered by pain"
},

{ MUT_AUGMENTATION,            0,  3, false, false, false,
  "augmentation",

  {"Your magical and physical power is slightly enhanced as your life falls.",
   "Your magical and physical power is enhanced as your life falls.",
   "Your magical and physical power is greatly enhanced as your life falls."},

  {"You feel power flowing into your body.",
   "You feel power rushing into your body.",
   "You feel saturated with power."},

  {"", "", ""},

  "augmentation"
},

// Jiyva only mutations
{ MUT_GELATINOUS_BODY,            0,  3, false, true, true,
  NULL,

  {N_("Your rubbery body absorbs attacks (AC +1)."),
   N_("Your pliable body absorbs attacks (AC +1, EV +1)."),
   N_("Your gelatinous body deflects attacks (AC +2, EV +2).")},

   {N_("Your body becomes stretchy."),
    N_("Your body becomes more malleable."),
    N_("Your body becomes viscous.")},

   {N_("Your body returns to its normal consistency."),
    N_("Your body becomes less malleable."),
    N_("Your body becomes less viscous.")},

    "gelatinous body"
},

{ MUT_EYEBALLS,                   0,  3, false, true, true,
  NULL,

  {N_("Your body is partially covered in golden eyeballs (Acc +3)."),
   N_("Your body is mostly covered in golden eyeballs (Acc +5)."),
   N_("Your body is completely covered in golden eyeballs (Acc +7, SInv).")},

   {N_("Eyeballs grow over part of your body."),
    N_("Eyeballs cover a large portion of your body."),
    N_("Eyeballs cover you completely.")},

   {N_("The eyeballs on your body disappear."),
    N_("The eyeballs on your body recede somewhat."),
    N_("The eyeballs on your body recede somewhat.")},

    "eyeballs"
},

{ MUT_TRANSLUCENT_SKIN,           0,  3, false, true, true,
  M_("translucent skin"),

  {N_("Your skin is partially translucent."),
   N_("Your skin is mostly translucent (Stlth)."),
   N_("Your transparent skin reduces the accuracy of your foes (Stlth).")},

  {N_("Your skin becomes partially translucent."),
   N_("Your skin becomes more translucent."),
   N_("Your skin becomes completely transparent.")},

  {N_("Your skin returns to its normal opacity."),
   N_("Your skin's translucency fades."),
   N_("Your skin's transparency fades.")},

   "translucent skin"
},

{ MUT_PSEUDOPODS,                 0,  3, false, true, true,
  M_("pseudopods"),

  {N_("Armour fits poorly on your pseudopods."),
   N_("Armour fits poorly on your large pseudopods."),
   N_("Armour fits poorly on your massive pseudopods.")},

  {N_("Pseudopods emerge from your body."),
   N_("Your pseudopods grow in size."),
   N_("Your pseudopods grow in size.")},

  {N_("Your pseudopods retract into your body."),
   N_("Your pseudopods become smaller."),
   N_("Your pseudopods become smaller.")},

   "pseudopods"
},

{ MUT_FOOD_JELLY,                       0,  1, false, true, false,
  M_("spawn jellies when eating"),

  {N_("You occasionally spawn a jelly by eating."), "", ""},
  {N_("You feel more connected to the slimes."), "", ""},
  {N_("Your connection to the slimes vanishes."), "", ""},

  "jelly spawner"
},

{ MUT_ACIDIC_BITE,                      0,  1, false, true, true,
  M_("acidic bite"),

  {N_("You have acidic saliva."), "", ""},
  {N_("Acid begins to drip from your mouth."), "", ""},
  {N_("Your mouth feels dry."), "", ""},

  "acidic bite"
},

// Scale mutations
{ MUT_DISTORTION_FIELD,                 2,  3, false, false, false,
  NULL,

  {N_("You are surrounded by a mild repulsion field (EV +2)."),
   N_("You are surrounded by a moderate repulsion field (EV +3)."),
   N_("You are surrounded by a strong repulsion field (EV +4, rMsl).")},

  {N_("You begin to radiate repulsive energy."),
   N_("Your repulsive radiation grows stronger."),
   N_("Your repulsive radiation grows stronger.")},

  {N_("You feel less repulsive."),
   N_("You feel less repulsive."),
   N_("You feel less repulsive.")},

  "repulsion field"
},

{ MUT_ICY_BLUE_SCALES,                  2,  3, false, true, true,
  NULL,

  {N_("You are partially covered in icy blue scales (AC +1)."),
   N_("You are mostly covered in icy blue scales (AC +2, EV -1)."),
   N_("You are completely covered in icy blue scales (AC +3, EV -1, rC+).")},

  {N_("Icy blue scales grow over part of your body."),
   N_("Icy blue scales spread over more of your body."),
   N_("Icy blue scales cover your body completely.")},

  {N_("Your icy blue scales disappear."),
   N_("Your icy blue scales recede somewhat."),
   N_("Your icy blue scales recede somewhat.")},

  "icy blue scales"
},

{ MUT_IRIDESCENT_SCALES,                2,  3, false,  true, true,
  NULL,

  {N_("You are partially covered in iridescent scales (AC +4)."),
   N_("You are mostly covered in iridescent scales (AC +6)."),
   N_("You are completely covered in iridescent scales (AC +8).")},

  {N_("Iridescent scales grow over part of your body."),
   N_("Iridescent scales spread over more of your body."),
   N_("Iridescent scales cover you completely.")},

  {N_("Your iridescent scales disappear."),
   N_("Your iridescent scales recede somewhat."),
   N_("Your iridescent scales recede somewhat.")},

  "iridescent scales"
},

{ MUT_LARGE_BONE_PLATES,                2,  3, false,  true, true,
  NULL,

  {N_("You are partially covered in large bone plates (AC +2, SH +2)."),
   N_("You are mostly covered in large bone plates (AC +3, SH +3)."),
   N_("You are completely covered in large bone plates (AC +4, SH +4).")},

  {N_("Large bone plates grow over parts of your arms."),
   N_("Large bone plates spread over more of your arms."),
   N_("Large bone plates cover your arms completely.")},

  {N_("Your large bone plates disappear."),
   N_("Your large bone plates recede somewhat."),
   N_("Your large bone plates recede somewhat.")},

  "large bone plates"
},

{ MUT_MOLTEN_SCALES,                    2,  3, false, true, true,
  NULL,

  {N_("You are partially covered in molten scales (AC +1)."),
   N_("You are mostly covered in molten scales (AC +2, EV -1)."),
   N_("You are completely covered in molten scales (AC +3, EV -1, rF+).")},

  {N_("Molten scales grow over part of your body."),
   N_("Molten scales spread over more of your body."),
   N_("Molten scales cover your body completely.")},

  {N_("Your molten scales disappear."),
   N_("Your molten scales recede somewhat."),
   N_("Your molten scales recede somewhat.")},

  "molten scales"
},

{ MUT_ROUGH_BLACK_SCALES,              2,  3, false,  true, true,
  NULL,

  {N_("You are partially covered in rough black scales (AC +4, Dex -1)."),
   N_("You are mostly covered in rough black scales (AC +7, Dex -2)."),
   N_("You are completely covered in rough black scales (AC +10, Dex -3).")},

  {N_("Rough black scales grow over part of your body."),
   N_("Rough black scales spread over more of your body."),
   N_("Rough black scales cover you completely.")},

  {N_("Your rough black scales disappear."),
   N_("Your rough black scales recede somewhat."),
   N_("Your rough black scales recede somewhat.")},

  "rough black scales"
},

{ MUT_RUGGED_BROWN_SCALES,              2,  3, false,  true, true,
  NULL,

  {N_("You are partially covered in rugged brown scales (AC +2, +3% HP)."),
   N_("You are mostly covered in rugged brown scales (AC +2, +5% HP)."),
   N_("You are completely covered in rugged brown scales (AC +2, +7% HP).")},

  {N_("Rugged brown scales grow over part of your body."),
   N_("Rugged brown scales spread over more of your body."),
   N_("Rugged brown scales cover you completely.")},

  {N_("Your rugged brown scales disappear."),
   N_("Your rugged brown scales recede somewhat."),
   N_("Your rugged brown scales recede somewhat.")},

  "rugged brown scales"
},

{ MUT_SLIMY_GREEN_SCALES,            2,  3, false, true, true,
  NULL,

  {N_("You are partially covered in slimy green scales (AC +1)."),
   N_("You are mostly covered in slimy green scales (AC +2)."),
   N_("You are completely covered in slimy green scales (AC +3, rPois).")},

  {N_("Slimy green scales grow over part of your body."),
   N_("Slimy green scales spread over more of your body."),
   N_("Slimy green scales cover your body completely.")},

  {N_("Your slimy green scales disappear."),
   N_("Your slimy green scales recede somewhat."),
   N_("Your slimy green scales recede somewhat.")},

  "slimy green scales"
},

{ MUT_THIN_METALLIC_SCALES,            2,  3, false, true, true,
  NULL,

  {N_("You are partially covered in thin metallic scales (AC +1)."),
   N_("You are mostly covered in thin metallic scales (AC +2)."),
   N_("You are completely covered in thin metallic scales (AC +3, rElec).")},

  {N_("Thin metallic scales grow over part of your body."),
   N_("Thin metallic scales spread over more of your body."),
   N_("Thin metallic scales cover your body completely.")},

  {N_("Your thin metallic scales disappear."),
   N_("Your thin metallic scales recede somewhat."),
   N_("Your thin metallic scales recede somewhat.")},

  "thin metallic scales"
},

{ MUT_THIN_SKELETAL_STRUCTURE,          2,  3, false,  true, false,
  NULL,

  {N_("You have a somewhat thin skeletal structure (Dex +2, Str -1, Stlth)."),
   N_("You have a moderately thin skeletal structure (Dex +4, Str -2, Stlth)."),
   N_("You have an unnaturally thin skeletal structure (Dex +6, Str -3, Stlth).")},

  {N_("Your bones become slightly less dense."),
   N_("Your bones become somewhat less dense."),
   N_("Your bones become less dense.")},

  {N_("Your skeletal structure returns to normal."),
   N_("Your skeletal structure densifies."),
   N_("Your skeletal structure densifies.")},

  "thin skeletal structure"
},

{ MUT_YELLOW_SCALES,                    2,  3, false,  true, true,
  NULL,

  {N_("You are partially covered in yellow scales (AC +1)."),
   N_("You are mostly covered in yellow scales (AC +2)."),
   N_("You are completely covered in yellow scales (AC +3, rCorr).")},

  {N_("Yellow scales grow over part of your body."),
   N_("Yellow scales spread over more of your body."),
   N_("Yellow scales cover you completely.")},

  {N_("Your yellow scales disappear."),
   N_("Your yellow scales recede somewhat."),
   N_("Your yellow scales recede somewhat.")},

  "yellow scales"
},

{ MUT_CAMOUFLAGE,           1,   3, false, true, true,
  M_("camouflage"),

  {N_("Your skin changes colour to match your surroundings (Stlth)."),
   N_("Your skin blends seamlessly with your surroundings (Stlth)."),
   N_("Your skin perfectly mimics your surroundings (Stlth).")},

  {N_("Your skin functions as natural camouflage."),
   N_("Your natural camouflage becomes more effective."),
   N_("Your natural camouflage becomes more effective.")},

  {N_("Your skin no longer functions as natural camouflage."),
   N_("Your natural camouflage becomes less effective."),
   N_("Your natural camouflage becomes less effective.")},

   "camouflage"
},

{ MUT_IGNITE_BLOOD,              0,  1, false, false, false,
  M_("ignite blood"),

  {N_("Your demonic aura causes spilled blood to erupt in flames."), "", ""},
  {N_("Your blood runs red-hot!"), "", ""},
  {"", "", ""},

  "ignite blood"
},

{ MUT_FOUL_STENCH,              0,  2, false, false, false,
  M_("foul stench"),

  {N_("You emit a foul stench, and are resistant to rotting and decay."),
   N_("You radiate miasma, and are resistant to rotting and decay."),
   ""},

  {N_("You begin to emit a foul stench of rot and decay."),
   N_("You begin to radiate miasma."),
   ""},

  {"", "", ""},

  "foul stench"
},

#endif
