// mutation definitions:
// first  number  = probability (0 means it doesn't appear naturally
//                  unless you have some level of it already)
// second number  = maximum levels
// first  boolean = is mutation mostly bad?
// second boolean = is mutation physical, i.e. external only?
// third  boolean = is mutation suppressed when shapechanged?
// first  string  = what to show in '%'
// second strings = what to show in 'A'
// third  strings = message given when gaining the mutation
// fourth strings = message given when losing the mutation
// fifth  string  = wizard-mode name of mutation

{ MUT_TOUGH_SKIN,                     0,  3, false,  true,  true,
  M_("tough skin"),

  {M_("You have tough skin (AC +1)."),
   M_("You have very tough skin (AC +2)."),
   M_("You have extremely tough skin (AC +3).")},

  {M_("Your skin toughens."),
   M_("Your skin toughens."),
   M_("Your skin toughens.")},

  {M_("Your skin feels delicate."),
   M_("Your skin feels delicate."),
   M_("Your skin feels delicate.")},

  "tough skin"
},

{ MUT_STRONG,                         7, 2, false,  true, false,
  NULL,

  {M_("Your muscles are strong. (Str +2)"),
   M_("Your muscles are very strong. (Str +4)"), ""},
  {"", "", ""},
  {"", "", ""},

  "strong"
},

{ MUT_CLEVER,                         7, 2, false,  true, false,
  NULL,

  {M_("Your mind is acute. (Int +2)"),
   M_("Your mind is very acute. (Int +4)"), ""},
  {"", "", ""},
  {"", "", ""},

  "clever"
},

{ MUT_AGILE,                          7, 2, false,  true, false,
  NULL,

  {M_("You are agile. (Dex +2)"),
   M_("You are very agile. (Dex +4)"), ""},
  {"", "", ""},
  {"", "", ""},

  "agile"
},

{ MUT_POISON_RESISTANCE,              4,  1, false, false,  true,
  M_("poison resistance"),

  {M_("Your system is resistant to poisons."), "", ""},
  {M_("You feel healthy."), "",  ""},
  {M_("You feel a little less healthy."), "", ""},

  "poison resistance"
},

{ MUT_CARNIVOROUS,                    5,  3, false, false, false,
  M_("carnivore"),

  {M_("Your digestive system is specialised to digest meat."),
   M_("Your digestive system is highly specialised to digest meat."),
   M_("You are carnivorous and can eat meat at any time.")},

  {M_("You hunger for flesh."),
   M_("You hunger for flesh."),
   M_("You hunger for flesh.")},

  {M_("You feel able to eat a more balanced diet."),
   M_("You feel able to eat a more balanced diet."),
   M_("You feel able to eat a more balanced diet.")},

  "carnivorous"
},

{ MUT_HERBIVOROUS,                    5,  3,  true, false, false,
  M_("herbivore"),

  {M_("You digest meat inefficiently."),
   M_("You digest meat very inefficiently."),
   M_("You are a herbivore.")},

  {M_("You hunger for vegetation."),
   M_("You hunger for vegetation."),
   M_("You hunger for vegetation.")},

  {M_("You feel able to eat a more balanced diet."),
   M_("You feel able to eat a more balanced diet."),
   M_("You feel able to eat a more balanced diet.")},

  "herbivorous"
},

{ MUT_HEAT_RESISTANCE,                4,  3, false, false,  true,
  M_("fire resistance"),

  {M_("Your flesh is heat resistant."),
   M_("Your flesh is very heat resistant."),
   M_("Your flesh is almost immune to the effects of heat.")},

  {M_("You feel a sudden chill."),
   M_("You feel a sudden chill."),
   M_("You feel a sudden chill.")},

  {M_("You feel hot for a moment."),
   M_("You feel hot for a moment."),
   M_("You feel hot for a moment.")},

  "heat resistance"
},

{ MUT_COLD_RESISTANCE,                4,  3, false, false,  true,
  M_("cold resistance"),

  {M_("Your flesh is cold resistant."),
   M_("Your flesh is very cold resistant."),
   M_("Your flesh is almost immune to the effects of cold.")},

  {M_("You feel hot for a moment."),
   M_("You feel hot for a moment."),
   M_("You feel hot for a moment.")},

  {M_("You feel a sudden chill."),
   M_("You feel a sudden chill."),
   M_("You feel a sudden chill.")},

  "cold resistance"
},

{ MUT_DEMONIC_GUARDIAN,               0,  3, false, false, false,
  M_("demonic guardian"),

  {M_("A weak demonic guardian rushes to your aid."),
   M_("A demonic guardian rushes to your aid."),
   M_("A powerful demonic guardian rushes to your aid.")},

  {M_("You feel the presence of a demonic guardian."),
   M_("Your guardian grows in power."),
   M_("Your guardian grows in power.")},

  {M_("Your demonic guardian is gone."),
   M_("Your demonic guardian is weakened."),
   M_("Your demonic guardian is weakened.")},

  "demonic guardian"
},

{ MUT_SHOCK_RESISTANCE,               2,  1, false, false,  true,
  M_("electricity resistance"),

  {M_("You are resistant to electric shocks."), "", ""},
  {M_("You feel insulated."), "", ""},
  {("You feel conductive."), "", ""},

  "shock resistance"
},

{ MUT_REGENERATION,                   3,  3, false, false, false,
  M_("regeneration"),

  {M_("Your natural rate of healing is unusually fast."),
   M_("You heal very quickly."),
   M_("You regenerate.")},

  {M_("You begin to heal more quickly."),
   M_("You begin to heal more quickly."),
   M_("You begin to regenerate.")},

  {M_("Your rate of healing slows."),
   M_("Your rate of healing slows."),
   M_("Your rate of healing slows.")},

  "regeneration"
},

{ MUT_SLOW_HEALING,                   3,  3,  true, false, false,
  M_("slow healing"),

  {M_("You heal slowly."),
   M_("You heal very slowly."),
   M_("You do not heal naturally.")},

  {M_("You begin to heal more slowly."),
   M_("You begin to heal more slowly."),
   M_("You stop healing.")},

  {M_("Your rate of healing increases."),
   M_("Your rate of healing increases."),
   M_("Your rate of healing increases.")},

  "slow healing"
},

{ MUT_FAST_METABOLISM,               10,  3,  true, false, false,
  M_("fast metabolism"),

  {M_("You have a fast metabolism."),
   M_("You have a very fast metabolism."),
   M_("Your metabolism is lightning-fast.")},

  {M_("You feel a little hungry."),
   M_("You feel a little hungry."),
   M_("You feel a little hungry.")},

  {M_("Your metabolism slows."),
   M_("Your metabolism slows."),
   M_("Your metabolism slows.")},

  "fast metabolism"
},

{ MUT_SLOW_METABOLISM,                7,  2, false, false, false,
  M_("slow metabolism"),

  {M_("You have a slow metabolism."),
   M_("You need consume almost no food."),
   ""},

  {M_("Your metabolism slows."),
   M_("Your metabolism slows."),
   ""},

  {M_("You feel a little hungry."),
   M_("You feel a little hungry."),
   ""},

  "slow metabolism"
},

{ MUT_WEAK,                          8, 2,  true,  true, false,
  NULL,
  {M_("You are weak. (Str -2)"),
   M_("You are very weak. (Str -4)"), ""},
  {"", "", ""},
  {"", "", ""},
  "weak"
},

{ MUT_DOPEY,                         8, 2,  true,  true, false,
  NULL,
  {M_("You are dopey. (Int -2)"),
   M_("You are very dopey. (Int -4)"), ""},
  {"", "", ""},
  {"", "", ""},
  "dopey",
},

{ MUT_CLUMSY,                        8, 2,  true,  true, false,
  NULL,
  {M_("You are clumsy. (Dex -2)"),
   M_("You are very clumsy. (Dex -4)"), ""},
  {"", "", ""},
  {"", "", ""},
  "clumsy"
},

#if TAG_MAJOR_VERSION == 34
{ MUT_TELEPORT_CONTROL,               0,  1, false, false, false,
  M_("teleport control"),

  {M_("You can control translocations."), "", ""},
  {M_("You feel controlled."), "", ""},
  {M_("You feel random."), "", ""},

  "teleport control"
},
#endif

{ MUT_TELEPORT,                       3,  3,  true, false, false,
  M_("teleportitis"),

  {M_("Space occasionally distorts in your vicinity."),
   M_("Space sometimes distorts in your vicinity."),
   M_("Space frequently distorts in your vicinity.")},

  {M_("You feel weirdly uncertain."),
   M_("You feel even more weirdly uncertain."),
   M_("You feel even more weirdly uncertain.")},

  {M_("You feel stable."),
   M_("You feel stable."),
   M_("You feel stable.")},

  "teleport"
},

{ MUT_MAGIC_RESISTANCE,               5,  3, false, false, false,
  M_("magic resistance"),

  {M_("You are resistant to hostile enchantments."),
   M_("You are highly resistant to hostile enchantments."),
   M_("You are extremely resistant to the effects of hostile enchantments.")},

  {M_("You feel resistant to hostile enchantments."),
   M_("You feel more resistant to hostile enchantments."),
   M_("You feel almost impervious to the effects of hostile enchantments.")},

  {M_("You feel less resistant to hostile enchantments."),
   M_("You feel less resistant to hostile enchantments."),
   M_("You feel vulnerable to magic hostile enchantments.")},

  "magic resistance"
},

{ MUT_FAST,                           1,  3, false, false,  true,
  M_("speed"),

  {M_("You cover ground quickly."),
   M_("You cover ground very quickly."),
   M_("You cover ground extremely quickly.")},

  {M_("You feel quick."),
   M_("You feel quick."),
   M_("You feel quick.")},

  {M_("You feel sluggish."),
   M_("You feel sluggish."),
   M_("You feel sluggish.")},

  "fast"
},

{ MUT_SLOW,                           3,  3,  true, false,  true,
  M_("slowness"),

  {M_("You cover ground slowly."),
   M_("You cover ground very slowly."),
   M_("You cover ground extremely slowly.")},

  {M_("You feel sluggish."),
   M_("You feel sluggish."),
   M_("You feel sluggish.")},

  {M_("You feel quick."),
   M_("You feel quick."),
   M_("You feel quick.")},

  "slow"
},

{ MUT_ACUTE_VISION,                   2,  1, false, false, false,
  M_("see invisible"),

  {M_("You have supernaturally acute eyesight."), "", ""},

  {M_("Your vision sharpens."),
   M_("Your vision sharpens."),
   M_("Your vision sharpens.")},

  {M_("Your vision seems duller."),
   M_("Your vision seems duller."),
   M_("Your vision seems duller.")},

  "acute vision"
},

{ MUT_DEFORMED,                       8,  1,  true,  true,  true,
  M_("deformed body"),

  {M_("Armour fits poorly on your strangely shaped body."), "", ""},
  {M_("Your body twists and deforms."), "", ""},
  {M_("Your body's shape seems more normal."), "", ""},

  "deformed"
},

{ MUT_SPIT_POISON,                    8,  3, false, false, false,
  M_("spit poison"),

  {M_("You can spit poison."),
   M_("You can spit moderately strong poison."),
   M_("You can spit strong poison.")},

  {M_("There is a nasty taste in your mouth for a moment."),
   M_("There is a nasty taste in your mouth for a moment."),
   M_("There is a nasty taste in your mouth for a moment.")},

  {M_("You feel an ache in your throat."),
   M_("You feel an ache in your throat."),
   M_("You feel an ache in your throat.")},

  "spit poison"
},

{ MUT_BREATHE_FLAMES,                 4,  3, false, false, false,
  M_("breathe flames"),

  {M_("You can breathe flames."),
   M_("You can breathe fire."),
   M_("You can breathe blasts of fire.")},

  {M_("Your throat feels hot."),
   M_("Your throat feels hot."),
   M_("Your throat feels hot.")},

  {M_("A chill runs up and down your throat."),
   M_("A chill runs up and down your throat."),
   M_("A chill runs up and down your throat.")},

  "breathe flames"
},

{ MUT_BLINK,                          3,  3, false, false, false,
  M_("blink"),

  {M_("You can translocate small distances at will."),
   M_("You are good at translocating small distances at will."),
   M_("You can easily translocate small distances at will.")},

  {M_("You feel jittery."),
   M_("You feel more jittery."),
   M_("You feel even more jittery.")},

  {M_("You feel a little less jittery."),
   M_("You feel less jittery."),
   M_("You feel less jittery.")},

  "blink"
},

#if TAG_MAJOR_VERSION == 34
{ MUT_STRONG_STIFF,                  0,  3, false,  true, false,
  NULL,

  {M_("Your muscles are strong, but stiff (Str +1, Dex -1)."),
   M_("Your muscles are very strong, but stiff (Str +2, Dex -2)."),
   M_("Your muscles are extremely strong, but stiff (Str +3, Dex -3).")},

  {M_("Your muscles feel sore."),
   M_("Your muscles feel sore."),
   M_("Your muscles feel sore.")},

  {M_("Your muscles feel loose."),
   M_("Your muscles feel loose."),
   M_("Your muscles feel loose.")},

  "strong stiff"
},

{ MUT_FLEXIBLE_WEAK,                 0,  3, false,  true, false,
  NULL,

  {M_("Your muscles are flexible, but weak (Str -1, Dex +1)."),
   M_("Your muscles are very flexible, but weak (Str -2, Dex +2)."),
   M_("Your muscles are extremely flexible, but weak (Str -3, Dex +3).")},

  {M_("Your muscles feel loose."),
   M_("Your muscles feel loose."),
   M_("Your muscles feel loose.")},

  {M_("Your muscles feel sore."),
   M_("Your muscles feel sore."),
   M_("Your muscles feel sore.")},

  "flexible weak"
},
#endif

{ MUT_SCREAM,                         6,  3,  true, false, false,
  M_("screaming"),

  {M_("You occasionally shout uncontrollably."),
   M_("You sometimes yell uncontrollably."),
   M_("You frequently scream uncontrollably.")},

  {M_("You feel the urge to shout."),
   M_("You feel a strong urge to yell."),
   M_("You feel a strong urge to scream.")},

  {M_("Your urge to shout disappears."),
   M_("Your urge to yell lessens."),
   M_("Your urge to scream lessens.")},

  "scream"
},

{ MUT_CLARITY,                        6,  1, false, false, false,
  M_("clarity"),

  {M_("You possess an exceptional clarity of mind."), "", ""},
  {M_("Your thoughts seem clearer."), "", ""},
  {M_("Your thinking seems confused."), "", ""},

  "clarity"
},

{ MUT_BERSERK,                        7,  3,  true, false, false,
  M_("berserk"),

  {M_("You tend to lose your temper in combat."),
   M_("You often lose your temper in combat."),
   M_("You have an uncontrollable temper.")},

  {M_("You feel a little pissed off."),
   M_("You feel angry."),
   M_("You feel extremely angry at everything!")},

  {M_("You feel a little more calm."),
   M_("You feel a little less angry."),
   M_("You feel a little less angry.")},

  "berserk"
},

{ MUT_DETERIORATION,                 10,  3,  true, false, false,
  M_("deterioration"),

  {M_("Your body is slowly deteriorating."),
   M_("Your body is deteriorating."),
   M_("Your body is rapidly deteriorating.")},

  {M_("You feel yourself wasting away."),
   M_("You feel yourself wasting away."),
   M_("You feel your body start to fall apart.")},

  {M_("You feel healthier."),
   M_("You feel a little healthier."),
   M_("You feel a little healthier.")},

  "deterioration"
},

{ MUT_BLURRY_VISION,                 10,  3,  true, false, false,
  M_("blurry vision"),

  {M_("Your vision is a little blurry."),
   M_("Your vision is quite blurry."),
   M_("Your vision is extremely blurry.")},

  {M_("Your vision blurs."),
   M_("Your vision blurs."),
   M_("Your vision blurs.")},

  {M_("Your vision sharpens."),
   M_("Your vision sharpens a little."),
   M_("Your vision sharpens a little.")},

  "blurry vision"
},

{ MUT_MUTATION_RESISTANCE,            4,  3, false, false, false,
  M_("mutation resistance"),

  {M_("You are somewhat resistant to further mutation."),
   M_("You are somewhat resistant to both further mutation and mutation removal."),
   M_("You are almost entirely resistant to further mutation and mutation removal.")},

  {M_("You feel genetically stable."),
   M_("You feel genetically stable."),
   M_("You feel genetically immutable.")},

  {M_("You feel genetically unstable."),
   M_("You feel genetically unstable."),
   M_("You feel genetically unstable.")},

  "mutation resistance"
},

{ MUT_EVOLUTION,                      4,  2, false, false, false,
  M_("evolution"),

  {M_("You evolve."),
   M_("You rapidly evolve."),
   ""},

  {M_("You feel nature experimenting on you. Don't worry, failures die fast."),
   M_("Your genes go into a fast flux."),
   ""},

  {M_("You feel genetically stable."),
   M_("Your wild genetic ride slows down."),
   ""},

  "evolution"
},

{ MUT_FRAIL,                         10,  3,  true,  true, false,
  NULL,

  {M_("You are frail (-10% HP)."),
   M_("You are very frail (-20% HP)."),
   M_("You are extremely frail (-30% HP).")},

  {M_("You feel frail."),
   M_("You feel frail."),
   M_("You feel frail.")},

  {M_("You feel robust."),
   M_("You feel robust."),
   M_("You feel robust.")},

  "frail"
},

{ MUT_ROBUST,                         5,  3, false,  true, false,
  NULL,

  {M_("You are robust (+10% HP)."),
   M_("You are very robust (+20% HP)."),
   M_("You are extremely robust (+30% HP).")},

  {M_("You feel robust."),
   M_("You feel robust."),
   M_("You feel robust.")},

  {M_("You feel frail."),
   M_("You feel frail."),
   M_("You feel frail.")},

  "robust"
},

{ MUT_UNBREATHING,                    0,  1, false, false,  true,
  M_("unbreathing"),

  {M_("You can survive without breathing."), "", ""},
  {M_("You feel breathless."), "", ""},
  {"", "", ""},

  "unbreathing"
},

{ MUT_TORMENT_RESISTANCE,             0,  1, false, false, false,
  M_("torment resistance"),

  {M_("You are immune to unholy pain and torment."), "", ""},
  {M_("You feel a strange anaesthesia."), "", ""},
  {"", "", ""},

  "torment resistance"
},

{ MUT_NEGATIVE_ENERGY_RESISTANCE,     0,  3, false, false, false,
  M_("life protection"),

  {M_("You resist negative energy."),
   M_("You are quite resistant to negative energy."),
   M_("You are immune to negative energy.")},

  {M_("You feel negative."),
   M_("You feel negative."),
   M_("You feel negative.")},

  {"", "", ""},

  "negative energy resistance"
},

{ MUT_HURL_HELLFIRE,                  0,  1, false, false, false,
  M_("hurl hellfire"),

  {M_("You can hurl blasts of hellfire."), "", ""},
  {M_("You smell fire and brimstone."), "", ""},
  {"", "", ""},

  "hurl hellfire"
},

// body-slot facets
{ MUT_HORNS,                          7,  3, false,  true,  true,
  M_("horns"),

  {M_("You have a pair of small horns on your head."),
   M_("You have a pair of horns on your head."),
   M_("You have a pair of large horns on your head.")},

  {M_("A pair of horns grows on your head!"),
   M_("The horns on your head grow some more."),
   M_("The horns on your head grow some more.")},

  {M_("The horns on your head shrink away."),
   M_("The horns on your head shrink a bit."),
   M_("The horns on your head shrink a bit.")},

  "horns"
},

{ MUT_BEAK,                           1,  1, false,  true,  true,
  M_("beak"),

  {M_("You have a beak for a mouth."), "", ""},
  {M_("Your mouth lengthens and hardens into a beak!"), "", ""},
  {M_("Your beak shortens and softens into a mouth."), "", ""},

  "beak"
},

{ MUT_CLAWS,                          2,  3, false,  true,  true,
  M_("claws"),

  {M_("You have sharp fingernails."),
   M_("You have very sharp fingernails."),
   M_("You have claws for hands.")},

  {M_("Your fingernails lengthen."),
   M_("Your fingernails sharpen."),
   M_("Your hands twist into claws.")},

  {M_("Your fingernails shrink to normal size."),
   M_("Your fingernails look duller."),
   M_("Your hands feel fleshier.")},

  "claws"
},

{ MUT_FANGS,                          1,  3, false,  true,  true,
  M_("fangs"),

  {M_("You have very sharp teeth."),
   M_("You have extremely sharp teeth."),
   M_("You have razor-sharp teeth.")},

  {M_("Your teeth lengthen and sharpen."),
   M_("Your teeth lengthen and sharpen some more."),
   M_("Your teeth are very long and razor-sharp.")},

  {M_("Your teeth shrink to normal size."),
   M_("Your teeth shrink and become duller."),
   M_("Your teeth shrink and become duller.")},

  "fangs"
},

{ MUT_HOOVES,                         5,  3, false,  true,  true,
  M_("hooves"),

  {M_("You have large cloven feet."),
   M_("You have hoof-like feet."),
   M_("You have hooves in place of feet.")},

  {M_("Your feet thicken and deform."),
   M_("Your feet thicken and deform."),
   M_("Your feet have mutated into hooves.")},

  {M_("Your hooves expand and flesh out into feet!"),
   M_("Your hooves look more like feet."),
   M_("Your hooves look more like feet.")},

  "hooves"
},

{ MUT_ANTENNAE,                       4,  3, false,  true,  true,
  M_("antennae"),

  {M_("You have a pair of small antennae on your head."),
   M_("You have a pair of antennae on your head."),
   M_("You have a pair of large antennae on your head (SInv).")},

  {M_("A pair of antennae grows on your head!"),
   M_("The antennae on your head grow some more."),
   M_("The antennae on your head grow some more.")},

  {M_("The antennae on your head shrink away."),
   M_("The antennae on your head shrink a bit."),
   M_("The antennae on your head shrink a bit.")},

  "antennae"
},

{ MUT_TALONS,                         5,  3, false,  true,  true,
  M_("talons"),

  {M_("You have sharp toenails."),
   M_("You have razor-sharp toenails."),
   M_("You have claws for feet.")},

  {M_("Your toenails lengthen and sharpen."),
   M_("Your toenails lengthen and sharpen."),
   M_("Your feet stretch into talons.")},

  {M_("Your talons dull and shrink into feet."),
   M_("Your talons look more like feet."),
   M_("Your talons look more like feet.")},

  "talons"
},

// Octopode only
{ MUT_TENTACLE_SPIKE,                10,  3, false,  true,  true,
  M_("spike"),

  {M_("One of your tentacles bears a spike."),
   M_("One of your tentacles bears a nasty spike."),
   M_("One of your tentacles bears a large vicious spike.")},

  {M_("One of your lower tentacles grows a sharp spike."),
   M_("Your tentacle spike grows bigger."),
   M_("Your tentacle spike grows even bigger.")},

  {M_("Your tentacle spike disappears."),
   M_("Your tentacle spike becomes smaller."),
   M_("Your tentacle spike recedes somewhat.")},

  "tentacle spike"
},

// Naga only; getting it is special-cased.
{ MUT_BREATHE_POISON,                 0,  1, false, false,  false,
  M_("breathe poison"),

  {M_("You can exhale a cloud of poison."), "", ""},
  {M_("You taste something nasty."), "", ""},
  {M_("Your breath is less nasty."), "", ""},

  "breathe poison"
},

// Naga and Draconian only
{ MUT_STINGER,                        8,  3, false,  true,  true,
  M_("stinger"),

  {M_("Your tail ends in a poisonous barb."),
   M_("Your tail ends in a sharp poisonous barb."),
   M_("Your tail ends in a wickedly sharp and poisonous barb.")},

  {M_("A poisonous barb forms on the end of your tail."),
   M_("The barb on your tail looks sharper."),
   M_("The barb on your tail looks very sharp.")},

  {M_("The barb on your tail disappears."),
   M_("The barb on your tail seems less sharp."),
   M_("The barb on your tail seems less sharp.")},

  "stinger"
},

// Draconian only
{ MUT_BIG_WINGS,                      4,  1, false,  true,  true,
  M_("large and strong wings"),

  {M_("Your wings are large and strong."), "", ""},
  {M_("Your wings grow larger and stronger."), "", ""},
  {M_("Your wings shrivel and weaken."), "", ""},

  "big wings"
},

// species-dependent innate mutations
{ MUT_SAPROVOROUS,                    0,  3, false, false, false,
  M_("saprovore"),

  {M_("You can tolerate rotten meat."),
   M_("You can eat rotten meat."),
   M_("You thrive on rotten meat.")},

  {M_("You hunger for rotting flesh."),
   M_("You hunger for rotting flesh."),
   M_("You hunger for rotting flesh.")},

  {"", "", ""},

  "saprovorous"
},

{ MUT_GOURMAND,                       0,  1, false, false, false,
  M_("gourmand"),

  {M_("You like to eat raw meat."), "", ""},
  {"", "", ""},
  {"", "", ""},

  "gourmand"
},

{ MUT_SHAGGY_FUR,                     2,  3, false,  true,  true,
  NULL,

  {M_("You are covered in fur (AC +1)."),
   M_("You are covered in thick fur (AC +2)."),
   M_("Your thick and shaggy fur keeps you warm (AC +3, rC+).")},

  {M_("Fur sprouts all over your body."),
   M_("Your fur grows into a thick mane."),
   M_("Your thick fur grows shaggy and warm.")},

  {M_("You shed all your fur."),
   M_("Your thick fur recedes somewhat."),
   M_("Your shaggy fur recedes somewhat.")},

  "shaggy fur"
},

{ MUT_HIGH_MAGIC,                     2,  3, false, false, false,
  NULL,

  {M_("You have an increased reservoir of magic (+10% MP)."),
   M_("You have a considerably increased reservoir of magic (+20% MP)."),
   M_("You have a greatly increased reservoir of magic (+30% MP).")},

  {M_("You feel more energetic."),
   M_("You feel more energetic."),
   M_("You feel more energetic.")},

  {M_("You feel less energetic."),
   M_("You feel less energetic."),
   M_("You feel less energetic.")},

  "high mp"
},

{ MUT_LOW_MAGIC,                      9,  3,  true, false, false,
  NULL,

  {M_("Your magical capacity is low (-10% MP)."),
   M_("Your magical capacity is very low (-20% MP)."),
   M_("Your magical capacity is extremely low (-30% MP).")},

  {M_("You feel less energetic."),
   M_("You feel less energetic."),
   M_("You feel less energetic.")},

  {M_("You feel more energetic."),
   M_("You feel more energetic."),
   M_("You feel more energetic.")},

  "low mp"
},

{ MUT_WILD_MAGIC,                     6,  3, false, false, false,
  M_("wild magic"),

  {M_("Your spells are a little harder to cast, but a little more powerful."),
   M_("Your spells are harder to cast, but more powerful."),
   M_("Your spells are much harder to cast, but much more powerful.")},

  {M_("You feel less in control of your magic."),
   M_("You feel less in control of your magic."),
   M_("You feel your magical power running wild!")},

  {M_("You regain control of your magic."),
   M_("You feel more in control of your magic."),
   M_("You feel more in control of your magic.")},

  "wild magic"
},

{ MUT_STOCHASTIC_TORMENT_RESISTANCE,  0,  1, false, false, false,
  M_("50% torment resistance"),

  {M_("You are somewhat able to resist unholy torments (1 in 2 success)."),"",""},
  {M_("You feel a strange anaesthesia."),"",""},
  {"","",""},

  "stochastic torment resistance"
},

{ MUT_PASSIVE_MAPPING,                3,  3, false, false, false,
  M_("sense surroundings"),

  {M_("You passively map a small area around you."),
   M_("You passively map the area around you."),
   M_("You passively map a large area around you.")},

  {M_("You feel a strange attunement to the structure of the dungeons."),
   M_("Your attunement to dungeon structure grows."),
   M_("Your attunement to dungeon structure grows further.")},

  {M_("You feel slightly disoriented."),
   M_("You feel slightly disoriented."),
   M_("You feel slightly disoriented.")},

  "passive mapping"
},

{ MUT_ICEMAIL,                        0,  1, false, false, false,
  NULL,

  {M_("A meltable icy envelope protects you from harm and freezing vapours (AC +"), "", ""},
  {M_("An icy envelope takes form around you."), "", ""},
  {"", "", ""},

  "icemail"
},

{ MUT_CONSERVE_SCROLLS,               0,  1, false, false, false,
  M_("conserve scrolls"),

  {M_("You are very good at protecting items from fire."), "", ""},
  {M_("You feel less concerned about heat."), "", ""},
  {"", "", ""},

  "conserve scrolls",
},

{ MUT_CONSERVE_POTIONS,               0,  1, false, false, false,
  M_("conserve potions"),

  {M_("You are very good at protecting items from cold."), "", ""},
  {M_("You feel less concerned about cold."), "", ""},
  {"", "", ""},
  "conserve potions",
},

{ MUT_PASSIVE_FREEZE,                 0,  1, false, false, false,
  M_("passive freeze"),

  {M_("A frigid envelope surrounds you and freezes all who hurt you."), "", ""},
  {M_("Your skin feels very cold."), "", ""},
  {"", "", ""},

  "passive freeze",
},

{ MUT_NIGHTSTALKER,                   0,  3, false,  true, false,
  M_("nightstalker"),

  {M_("You are slightly more attuned to the shadows."),
   M_("You are significantly more attuned to the shadows."),
   M_("You are completely attuned to the shadows.")},

  {M_("You slip into the darkness of the dungeon."),
   M_("You slip further into the darkness."),
   M_("You are surrounded by darkness.")},

  {M_("Your affinity for the darkness vanishes."),
   M_("Your affinity for the darkness weakens."),
   M_("Your affinity for the darkness weakens.")},

  "nightstalker"
},

{ MUT_SPINY,                          0,  3, false,  true,  true,
  M_("spiny"),

  {M_("You are partially covered in sharp spines."),
   M_("You are mostly covered in sharp spines."),
   M_("You are completely covered in sharp spines.")},

  {M_("Sharp spines emerge from parts of your body."),
   M_("Sharp spines emerge from more of your body."),
   M_("Sharp spines emerge from your entire body.")},

  {M_("Your sharp spines disappear entirely."),
   M_("Your sharp spines retract somewhat."),
   M_("Your sharp spines retract somewhat.")},

  "spiny"
},

{ MUT_POWERED_BY_DEATH,               0,  3, false, false, false,
  M_("powered by death"),

  {M_("You can steal the life force of nearby defeated enemies."),
   M_("You can steal the life force of defeated enemies."),
   M_("You can steal the life force of all defeated enemies in sight.")},

  {M_("A wave of death washes over you."),
   M_("The wave of death grows in power."),
   M_("The wave of death grows in power.")},

  {M_("Your control of surrounding life forces is gone."),
   M_("Your control of surrounding life forces weakens."),
   M_("Your control of surrounding life forces weakens.")},

  "powered by death"
},

{ MUT_POWERED_BY_PAIN,                0,  3, false, false, false,
  M_("powered by pain"),

  {M_("You sometimes gain a little power by taking damage."),
   M_("You sometimes gain power by taking damage."),
   M_("You are powered by pain.")},

  {M_("You feel energised by your suffering."),
   M_("You feel even more energised by your suffering."),
   M_("You feel completely energised by your suffering.")},

  {"", "", ""},

  "powered by pain"
},

{ MUT_AUGMENTATION,                   0,  3, false, false, false,
  M_("augmentation"),

  {M_("Your magical and physical power is slightly enhanced at high health."),
   M_("Your magical and physical power is enhanced at high health."),
   M_("Your magical and physical power is greatly enhanced at high health.")},

  {M_("You feel power flowing into your body."),
   M_("You feel power rushing into your body."),
   M_("You feel saturated with power.")},

  {"", "", ""},

  "augmentation"
},

{ MUT_MANA_SHIELD,                    0,  1, false, false, false,
  M_("magic shield"),

  {M_("When hurt, damage is shared between your health and your magic reserves."), "", ""},
  {M_("You feel your magical essence form a protective shroud around your flesh."), "", ""},
  {"", "", ""},

  "magic shield"
},

{ MUT_MANA_REGENERATION,              0,  1, false, false, false,
  M_("magic regeneration"),

  {M_("You regenerate magic rapidly."), "", ""},
  {M_("You feel your magic shroud grow more resilient."), "", ""},
  {"", "", ""},

  "magic regeneration"
},

{ MUT_MANA_LINK,                      0,  1, false, false, false,
  M_("magic link"),

  {M_("When low on magic, you restore magic in place of health."), "", ""},
  {M_("You feel your life force and your magical essence meld."), "", ""},
  {"", "", ""},

  "magic link"
},

// Jiyva only mutations
{ MUT_GELATINOUS_BODY,                0,  3, false,  true,  true,
  NULL,

  {M_("Your rubbery body absorbs attacks (AC +1)."),
   M_("Your pliable body absorbs attacks (AC +1, EV +1)."),
   M_("Your gelatinous body deflects attacks (AC +2, EV +2).")},

  {M_("Your body becomes stretchy."),
   M_("Your body becomes more malleable."),
   M_("Your body becomes viscous.")},

  {M_("Your body returns to its normal consistency."),
   M_("Your body becomes less malleable."),
   M_("Your body becomes less viscous.")},

  "gelatinous body"
},

{ MUT_EYEBALLS,                       0,  3, false,  true,  true,
  NULL,

  {M_("Your body is partially covered in golden eyeballs (Acc +3)."),
   M_("Your body is mostly covered in golden eyeballs (Acc +5)."),
   M_("Your body is completely covered in golden eyeballs (Acc +7, SInv).")},

  {M_("Eyeballs grow over part of your body."),
   M_("Eyeballs cover a large portion of your body."),
   M_("Eyeballs cover you completely.")},

  {M_("The eyeballs on your body disappear."),
   M_("The eyeballs on your body recede somewhat."),
   M_("The eyeballs on your body recede somewhat.")},

  "eyeballs"
},

{ MUT_TRANSLUCENT_SKIN,               0,  3, false,  true,  true,
  M_("translucent skin"),

  {M_("Your skin is partially translucent."),
   M_("Your skin is mostly translucent (Stlth)."),
   M_("Your transparent skin reduces the accuracy of your foes (Stlth).")},

  {M_("Your skin becomes partially translucent."),
   M_("Your skin becomes more translucent."),
   M_("Your skin becomes completely transparent.")},

  {M_("Your skin returns to its normal opacity."),
   M_("Your skin's translucency fades."),
   M_("Your skin's transparency fades.")},

  "translucent skin"
},

{ MUT_PSEUDOPODS,                     0,  3, false,  true,  true,
  M_("pseudopods"),

  {M_("Armour fits poorly on your pseudopods."),
   M_("Armour fits poorly on your large pseudopods."),
   M_("Armour fits poorly on your massive pseudopods.")},

  {M_("Pseudopods emerge from your body."),
   M_("Your pseudopods grow in size."),
   M_("Your pseudopods grow in size.")},

  {M_("Your pseudopods retract into your body."),
   M_("Your pseudopods become smaller."),
   M_("Your pseudopods become smaller.")},

  "pseudopods"
},

#if TAG_MAJOR_VERSION == 34
{ MUT_FOOD_JELLY,                     0,  1, false,  true, false,
  M_("spawn jellies when eating"),

  {M_("You occasionally spawn a jelly by eating."), "", ""},
  {M_("You feel more connected to the slimes."), "", ""},
  {M_("Your connection to the slimes vanishes."), "", ""},

  "jelly spawner"
},
#endif

{ MUT_ACIDIC_BITE,                    0,  1, false,  true,  true,
  M_("acidic bite"),

  {M_("You have acidic saliva."), "", ""},
  {M_("Acid begins to drip from your mouth."), "", ""},
  {M_("Your mouth feels dry."), "", ""},

  "acidic bite"
},

// Scale mutations
{ MUT_DISTORTION_FIELD,               2,  3, false, false, false,
  NULL,

  {M_("You are surrounded by a mild repulsion field (EV +2)."),
   M_("You are surrounded by a moderate repulsion field (EV +3)."),
   M_("You are surrounded by a strong repulsion field (EV +4, rMsl).")},

  {M_("You begin to radiate repulsive energy."),
   M_("Your repulsive radiation grows stronger."),
   M_("Your repulsive radiation grows stronger.")},

  {M_("You feel less repulsive."),
   M_("You feel less repulsive."),
   M_("You feel less repulsive.")},

  "repulsion field"
},

{ MUT_ICY_BLUE_SCALES,                2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in icy blue scales (AC +1)."),
   M_("You are mostly covered in icy blue scales (AC +3, EV -1)."),
   M_("You are completely covered in icy blue scales (AC +4, EV -1, rC+).")},

  {M_("Icy blue scales grow over part of your body."),
   M_("Icy blue scales spread over more of your body."),
   M_("Icy blue scales cover your body completely.")},

  {M_("Your icy blue scales disappear."),
   M_("Your icy blue scales recede somewhat."),
   M_("Your icy blue scales recede somewhat.")},

  "icy blue scales"
},

{ MUT_IRIDESCENT_SCALES,              2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in iridescent scales (AC +4)."),
   M_("You are mostly covered in iridescent scales (AC +6)."),
   M_("You are completely covered in iridescent scales (AC +8).")},

  {M_("Iridescent scales grow over part of your body."),
   M_("Iridescent scales spread over more of your body."),
   M_("Iridescent scales cover you completely.")},

  {M_("Your iridescent scales disappear."),
   M_("Your iridescent scales recede somewhat."),
   M_("Your iridescent scales recede somewhat.")},

  "iridescent scales"
},

{ MUT_LARGE_BONE_PLATES,              2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in large bone plates (AC +2, SH +2)."),
   M_("You are mostly covered in large bone plates (AC +3, SH +4)."),
   M_("You are completely covered in large bone plates (AC +4, SH +6).")},

  {M_("Large bone plates grow over parts of your arms."),
   M_("Large bone plates spread over more of your arms."),
   M_("Large bone plates cover your arms completely.")},

  {M_("Your large bone plates disappear."),
   M_("Your large bone plates recede somewhat."),
   M_("Your large bone plates recede somewhat.")},

  "large bone plates"
},

{ MUT_MOLTEN_SCALES,                  2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in molten scales (AC +1)."),
   M_("You are mostly covered in molten scales (AC +3, EV -1)."),
   M_("You are completely covered in molten scales (AC +4, EV -1, rF+).")},

  {M_("Molten scales grow over part of your body."),
   M_("Molten scales spread over more of your body."),
   M_("Molten scales cover your body completely.")},

  {M_("Your molten scales disappear."),
   M_("Your molten scales recede somewhat."),
   M_("Your molten scales recede somewhat.")},

  "molten scales"
},

{ MUT_ROUGH_BLACK_SCALES,             2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in rough black scales (AC +4, Dex -1)."),
   M_("You are mostly covered in rough black scales (AC +7, Dex -2)."),
   M_("You are completely covered in rough black scales (AC +10, Dex -3).")},

  {M_("Rough black scales grow over part of your body."),
   M_("Rough black scales spread over more of your body."),
   M_("Rough black scales cover you completely.")},

  {M_("Your rough black scales disappear."),
   M_("Your rough black scales recede somewhat."),
   M_("Your rough black scales recede somewhat.")},

  "rough black scales"
},

{ MUT_RUGGED_BROWN_SCALES,            2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in rugged brown scales (AC +1, +3% HP)."),
   M_("You are mostly covered in rugged brown scales (AC +2, +5% HP)."),
   M_("You are completely covered in rugged brown scales (AC +3, +7% HP).")},

  {M_("Rugged brown scales grow over part of your body."),
   M_("Rugged brown scales spread over more of your body."),
   M_("Rugged brown scales cover you completely.")},

  {M_("Your rugged brown scales disappear."),
   M_("Your rugged brown scales recede somewhat."),
   M_("Your rugged brown scales recede somewhat.")},

  "rugged brown scales"
},

{ MUT_SLIMY_GREEN_SCALES,             2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in slimy green scales (AC +2)."),
   M_("You are mostly covered in slimy green scales (AC +3)."),
   M_("You are completely covered in slimy green scales (AC +4, rPois).")},

  {M_("Slimy green scales grow over part of your body."),
   M_("Slimy green scales spread over more of your body."),
   M_("Slimy green scales cover your body completely.")},

  {M_("Your slimy green scales disappear."),
   M_("Your slimy green scales recede somewhat."),
   M_("Your slimy green scales recede somewhat.")},

  "slimy green scales"
},

{ MUT_THIN_METALLIC_SCALES,           2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in thin metallic scales (AC +2)."),
   M_("You are mostly covered in thin metallic scales (AC +3)."),
   M_("You are completely covered in thin metallic scales (AC +4, rElec).")},

  {M_("Thin metallic scales grow over part of your body."),
   M_("Thin metallic scales spread over more of your body."),
   M_("Thin metallic scales cover your body completely.")},

  {M_("Your thin metallic scales disappear."),
   M_("Your thin metallic scales recede somewhat."),
   M_("Your thin metallic scales recede somewhat.")},

  "thin metallic scales"
},

{ MUT_THIN_SKELETAL_STRUCTURE,        2,  3, false,  true, false,
  NULL,

  {M_("You have a somewhat thin skeletal structure (Dex +2, Stlth)."),
   M_("You have a moderately thin skeletal structure (Dex +4, Str -1, Stlth)."),
   M_("You have an unnaturally thin skeletal structure (Dex +6, Str -2, Stlth).")},

  {M_("Your bones become slightly less dense."),
   M_("Your bones become somewhat less dense."),
   M_("Your bones become less dense.")},

  {M_("Your skeletal structure returns to normal."),
   M_("Your skeletal structure densifies."),
   M_("Your skeletal structure densifies.")},

  "thin skeletal structure"
},

{ MUT_YELLOW_SCALES,                  2,  3, false,  true,  true,
  NULL,

  {M_("You are partially covered in yellow scales (AC +2)."),
   M_("You are mostly covered in yellow scales (AC +3)."),
   M_("You are completely covered in yellow scales (AC +4, rCorr).")},

  {M_("Yellow scales grow over part of your body."),
   M_("Yellow scales spread over more of your body."),
   M_("Yellow scales cover you completely.")},

  {M_("Your yellow scales disappear."),
   M_("Your yellow scales recede somewhat."),
   M_("Your yellow scales recede somewhat.")},

  "yellow scales"
},

{ MUT_CAMOUFLAGE,                     1,  3, false,  true,  true,
  M_("camouflage"),

  {M_("Your skin changes colour to match your surroundings (Stlth)."),
   M_("Your skin blends seamlessly with your surroundings (Stlth)."),
   M_("Your skin perfectly mimics your surroundings (Stlth).")},

  {M_("Your skin functions as natural camouflage."),
   M_("Your natural camouflage becomes more effective."),
   M_("Your natural camouflage becomes more effective.")},

  {M_("Your skin no longer functions as natural camouflage."),
   M_("Your natural camouflage becomes less effective."),
   M_("Your natural camouflage becomes less effective.")},

   "camouflage"
},

{ MUT_IGNITE_BLOOD,                   0,  1, false, false, false,
  M_("ignite blood"),

  {M_("Your demonic aura causes spilled blood to erupt in flames."), "", ""},
  {M_("Your blood runs red-hot!"), "", ""},
  {"", "", ""},

  "ignite blood"
},

{ MUT_FOUL_STENCH,                    0,  2, false, false, false,
  M_("foul stench"),

  {M_("You emit a foul stench, and are resistant to rotting and decay."),
   M_("You radiate miasma, and are resistant to rotting and decay."),
   ""},

  {M_("You begin to emit a foul stench of rot and decay."),
   M_("You begin to radiate miasma."),
   ""},

  {"", "", ""},

  "foul stench"
},

{ MUT_TENDRILS,                       0,  1, false,  true, true,
  M_("tendrils"),

  {M_("Thin tendrils of slime have grown from your body."), "", ""},
  {M_("Thin, slimy tendrils emerge from your body."), "", ""},
  {M_("Your tendrils retract into your body."), "", ""},

  "tendrils"
},

{ MUT_JELLY_GROWTH,                       0,  1, false,  true, true,
  M_("a jelly is attached to you"),

  {M_("You have a small jelly attached to you that senses nearby items."), "", ""},
  {M_("Your body partially splits into a small jelly."), "", ""},
  {M_("The jelly growth is reabsorbed into your body."), "", ""},

  "jelly growth"
},

{ MUT_JELLY_MISSILE,                       0,  1, false,  true, true,
  M_("absorbing missiles"),

  {M_("You have a small jelly attached to you that may absorb incoming projectiles."), "", ""},
  {M_("Your body partially splits into a small jelly."), "", ""},
  {M_("The jelly growth is reabsorbed into your body."), "", ""},

  "jelly missile"
},

{ MUT_PETRIFICATION_RESISTANCE,            0,  1, false, false, false,
  M_("petrification resistance"),

  {M_("You are immune to petrification."), "", ""},
  {M_("Your body vibrates."), "", ""},
  {M_("You briefly stop moving."), "", ""},

  "petrification resistance"
},

#if TAG_MAJOR_VERSION == 34
{ MUT_TRAMPLE_RESISTANCE,                  0,  1, false, false, false,
  M_("trample resistance"),

  {M_("You are resistant to trampling."), "", ""},
  {M_("You feel steady."), "", ""},
  {M_("You feel unsteady.."), "", ""},

  "trample resistance"
},

{ MUT_CLING,                               0,  1, false, false, true,
  M_("cling"),

  {M_("You can cling to walls."), "", ""},
  {M_("You feel sticky."), "", ""},
  {M_("You feel slippery."), "", ""},

  "cling"
},
#endif

{ MUT_FUMES,            0,  2, false, false, false,
  "fuming",

  {"You emit clouds of smoke.", "You frequently emit clouds of smoke.", ""},
  {"You fume.", "You fume more.", ""},
  {"You stop fuming.", "You fume less.", ""},

  "fumes"
}
