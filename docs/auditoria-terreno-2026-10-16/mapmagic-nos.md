# MapMagic — catálogo extraído de nós e parâmetros

Extração estática dos atributos GeneratorMenu e declarações públicas de cada classe. Inclui portas, propriedades auxiliares e variantes legadas/ocultas: não é contagem de nós únicos prontos na Astra. Métodos/controles adicionais estão em declaracoes-controles.csv. Implementação na Astra: **pendente** para todos os nós desta lista.

##  / Function Outdated — Function200

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/Function.cs:229`.
Atributos: `name ="Function Outdated", iconName="GeneratorIcons/Function", priority = 1, colorType = typeof(IBiome), disabled = true`.

- `public Graph srcGraph;`

##  / Generic Portal Enter — PortalEnter

Fonte: `Assets/MapMagic/Nodes/Portals.cs:41`.
Atributos: `name = "Generic Portal Enter"`.

- `public string name = "Portal";`
- `public string Name { get{ return name; } set{ name = value; } }`

##  / Generic Portal Exit — PortalExit

Fonte: `Assets/MapMagic/Nodes/Portals.cs:52`.
Atributos: `name ="Generic Portal Exit"`.

- `public PortalEnter<T> enter;`
- `public IPortalEnter<T> Enter => enter;`

##  / Random — Random200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsInitial.cs:169`.
Atributos: `menu=null, name ="Random", iconName="GeneratorIcons/Random", disengageable = true, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Scatter"`.


##  / Unknown — InletOutletPlaceholder

Fonte: `Assets/MapMagic/Nodes/Placeholders.cs:102`.
Atributos: `name = "Unknown", iconName="GeneratorIcons/Generator"`.


##  / Unknown — InletPlaceholder

Fonte: `Assets/MapMagic/Nodes/Placeholders.cs:105`.
Atributos: `name = "Unknown", iconName="GeneratorIcons/Generator"`.


##  / Unknown — OutletPlaceholder

Fonte: `Assets/MapMagic/Nodes/Placeholders.cs:108`.
Atributos: `name = "Unknown", iconName="GeneratorIcons/Generator"`.


##  / Unknown — Placeholder

Fonte: `Assets/MapMagic/Nodes/Placeholders.cs:111`.
Atributos: `name = "Unknown", iconName="GeneratorIcons/Generator"`.


##  / VS Pro Maps — VSProMapsOut

Fonte: `Assets/MapMagic/Compatibility/VegetationStudio/Placeholders/VSProMapsPlaceholder.cs:18`.
Atributos: `name = "VS Pro Maps", section =2, drawButtons = false, colorType = typeof(MatrixWorld)`.

- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public float density = 0.5f;`
- `public int maskGroup = 0;`
- `public int textureChannel = 0;`

##  / VS Pro Objs — VSProObjectsOut

Fonte: `Assets/MapMagic/Compatibility/VegetationStudioObjects/Placeholders/VSProObjectsPlaceholder.cs:19`.
Atributos: `name = "VS Pro Objs", section =2, drawButtons = false, colorType = typeof(TransitionsList)`.

- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public PositioningSettings posSettings = null;`
- `public BiomeBlend biomeBlend = BiomeBlend.Random;`
- `public string id;`
- `public string lastUsedName;`
- `public string lastUsedType;`
- `public Layer[] layers = new Layer[] { new Layer() };`
- `public bool objHeight = true;`
- `public bool relativeHeight = true;`
- `public bool guiHeight;`
- `public bool useRotation = true;`
- `public bool takeTerrainNormal = false;`
- `public bool rotateYonly = false;`
- `public bool regardPrefabRotation = false;`
- `public bool guiRotation;`
- `public bool useScale = true;`
- `public bool scaleYonly = false;`
- `public bool regardPrefabScale = false;`
- `public bool guiScale;`

## Biomes / Biomes Set — BiomesSet200

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/BiomesSet.cs:40`.
Atributos: `menu="Biomes", name ="Biomes Set", iconName="GeneratorIcons/Biome", priority = 1, colorType = typeof(IBiome)`.

- `public BiomeLayer[] layers = new BiomeLayer[0];`
- `public IList<IUnit> Layers { get => layers; set => layers=ArrayTools.Convert<BiomeLayer,IUnit>(value); }`
- `public bool Inversed => true;`
- `public bool HideFirst => true;`

## Biomes / Whittaker — Whittaker200

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/Whittaker.cs:57`.
Atributos: `menu="Biomes", name ="Whittaker", iconName="GeneratorIcons/Whittaker", priority = 1, colorType = typeof(IBiome)`.

- `[Val("Sharpness")] public float sharpness = 0.6f;`
- `public WhittakerLayer[] layers = new WhittakerLayer[]`
- `public IList<IUnit> Layers { get => layers; set {return;} }`
- `public bool Inversed => false;`
- `public bool HideFirst => false;`

## Brush/Input / Grass — BrushReadGrassSet

Fonte: `Assets/MapMagic/Brush/Generators/GrassInOut.cs:20`.
Atributos: `menu = "Brush/Input", name = "Grass", section=2, colorType = typeof(MatrixSet), iconName="GeneratorIcons/BrushGrassIn", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/GrassInOut"`.

- `public string Name { get => "Grass"; set {} }`

## Brush/Input / Height — BrushReadHeight206

Fonte: `Assets/MapMagic/Brush/Generators/HeightInOut.cs:20`.
Atributos: `menu = "Brush/Input", name = "Height", section=2, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/BrushHeightIn", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/HeightInOut"`.

- `public string Name { get => "Height"; set {} }`

## Brush/Input / Objects — BrushReadObjects

Fonte: `Assets/MapMagic/Brush/Generators/ObjectsInOut.cs:20`.
Atributos: `menu = "Brush/Input", name = "Objects", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/BrushObjectsIn", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/ObjectsInOut"`.

- `public bool specificPrefabs = false;`
- `public GameObject[] prefabs = new GameObject[1];`
- `public string Name { get => "Objects"; set {} }`

## Brush/Input / Textures — BrushReadTextureSet

Fonte: `Assets/MapMagic/Brush/Generators/TexturesSetsInOut.cs:20`.
Atributos: `menu = "Brush/Input", name = "Textures", section=2, colorType = typeof(MatrixSet), iconName="GeneratorIcons/BrushTexturesIn", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/HeightInOut"`.

- `public string Name { get => "Maps Set"; set {} }`

## Brush/Input / Trees — BrushReadTrees

Fonte: `Assets/MapMagic/Brush/Generators/TreesInOut.cs:20`.
Atributos: `menu = "Brush/Input", name = "Trees", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/TreesIn", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/TreesInOut"`.

- `public bool specificPrefabs = false;`
- `public GameObject[] prefabs = new GameObject[1];`
- `public string Name { get => "Trees"; set {} }`

## Brush/Output / Grass — BrushWriteGrassSet

Fonte: `Assets/MapMagic/Brush/Generators/GrassInOut.cs:82`.
Atributos: `menu = "Brush/Output", name = "Grass", section=2, colorType = typeof(MatrixSet), iconName="GeneratorIcons/BrushGrassOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/GrassInOut"`.

- `public string Name { get => "Grass"; set {} }`

## Brush/Output / Height — BrushWriteHeight206

Fonte: `Assets/MapMagic/Brush/Generators/HeightInOut.cs:104`.
Atributos: `menu = "Brush/Output", name = "Height", section=2, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/BrushHeightOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/HeightInOut"`.

- `public string Name { get => "Height"; set {} }`

## Brush/Output / Objects — BrushWriteObjects

Fonte: `Assets/MapMagic/Brush/Generators/ObjectsInOut.cs:125`.
Atributos: `menu = "Brush/Output", name = "Objects", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/BrushObjectsOut", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/ObjectsInOut"`.

- `public GameObject[] prefabs = new GameObject[1];`
- `public PositioningSettings posSettings = new PositioningSettings() {relativeHeight=false};`
- `public bool instantiateClones = false;`
- `public bool guiProperties;`
- `public string Name { get => "Objects"; set {} }`

## Brush/Output / Textures — BrushWriteTextureSet

Fonte: `Assets/MapMagic/Brush/Generators/TexturesSetsInOut.cs:75`.
Atributos: `menu = "Brush/Output", name = "Textures", section=2, colorType = typeof(MatrixSet), iconName="GeneratorIcons/BrushTexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/HeightInOut"`.

- `public string Name { get => "Maps Set"; set {} }`

## Brush/Output / Transfer Objects — BrushTransferObjects

Fonte: `Assets/MapMagic/Brush/Generators/ObjectsInOut.cs:70`.
Atributos: `menu = "Brush/Output", name = "Transfer Objects", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/BrushObjectsOut", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `public bool specificPrefabs = false;`
- `public GameObject[] prefabs = new GameObject[1];`
- `public PositioningSettings posSettings = new PositioningSettings() { relativeHeight = false };`
- `public string Name { get => "Objects"; set {} }`

## Brush/Output / Transfer Trees — BrushTransferTrees

Fonte: `Assets/MapMagic/Brush/Generators/TreesInOut.cs:60`.
Atributos: `menu = "Brush/Output", name = "Transfer Trees", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/BrushTreesOut", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/TreesInOut"`.

- `public bool specificPrefabs = false;`
- `public GameObject[] prefabs = new GameObject[1];`
- `public PositioningSettings posSettings = new PositioningSettings() { relativeHeight = false };`
- `public bool guiProperties;`
- `public Color color = Color.white;`
- `public Color lightmapColor = Color.white;`
- `public float bendFactor;`
- `public string Name { get => "Move Trees"; set {} }`

## Brush/Output / Write Trees — BrushWriteTrees

Fonte: `Assets/MapMagic/Brush/Generators/TreesInOut.cs:127`.
Atributos: `menu = "Brush/Output", name = "Write Trees", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/BrushTreesOut", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/Brush/TreesInOut"`.

- `public GameObject[] prefabs = new GameObject[1];`
- `public PositioningSettings posSettings = new PositioningSettings() { relativeHeight = false };`
- `public bool guiProperties;`
- `public bool useOriginalPrototype = false;`
- `public Color color = Color.white;`
- `public Color lightmapColor = Color.white;`
- `public float bendFactor;`
- `public string Name { get => "Trees"; set {} }`

## Functions / Function — Function210

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/Function.cs:37`.
Atributos: `menu="Functions", name ="Function", iconName="GeneratorIcons/Function", priority = 1, colorType = typeof(IBiome)`.

- `public float Complexity => subGraph!=null ? subGraph.GetGenerateComplexity() : 0;`

## Functions / Loop — Loop210

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/Loop.cs:16`.
Atributos: `menu="Functions", name ="Loop", iconName="GeneratorIcons/Function", priority = 1, colorType = typeof(IBiome)`.

- `public int iterations = 1;`
- `public float Complexity => subGraph!=null ? subGraph.GetGenerateComplexity()*iterations : 0;`

## Functions/Enter / Enter — MatrixFnEnter

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/FunctionPortals.cs:13`.
Atributos: `menu = "Functions/Enter", menuName = "Map", name = "Enter", iconName = "GeneratorIcons/FunctionIn", lookLikePortal=true`.


## Functions/Enter / Enter — ObjectsFnEnter

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/FunctionPortals.cs:15`.
Atributos: `menu = "Functions/Enter", menuName = "Objects", name = "Enter", iconName = "GeneratorIcons/FunctionIn", lookLikePortal=true`.


## Functions/Enter / Enter — SplineFunctionInput

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/FunctionPortals.cs:19`.
Atributos: `menu = "Functions/Enter", menuName = "Spline", name = "Enter", iconName = "GeneratorIcons/FunctionIn", lookLikePortal=true`.


## Functions/Exit / Exit — MatrixFnExit

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/FunctionPortals.cs:14`.
Atributos: `menu = "Functions/Exit", menuName = "Map", name = "Exit", iconName = "GeneratorIcons/FunctionOut", lookLikePortal=true`.


## Functions/Exit / Exit — ObjectsFnExit

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/FunctionPortals.cs:16`.
Atributos: `menu = "Functions/Exit", menuName = "Objects", name = "Exit", iconName = "GeneratorIcons/FunctionOut", lookLikePortal=true`.


## Functions/Exit / Exit — SplineFunctionOutput

Fonte: `Assets/MapMagic/Generators/Biomes/Runtime/FunctionPortals.cs:20`.
Atributos: `menu = "Functions/Exit", menuName = "Spline", name = "Exit", iconName = "GeneratorIcons/FunctionOut", lookLikePortal=true`.


## Map Set / Add — Add210

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsSpecial.cs:19`.
Atributos: `menu = "Map Set", name = "Add", colorType = typeof(MatrixSet), iconName="GeneratorIcons/MapSetAdd", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixSetGenerators/Add"`.

- `public PrototypeType prototypeType = PrototypeType.TerrainLayer;`
- `public TerrainLayer terrainLayer = null;`
- `public Texture2D texture = null;`
- `public GameObject prefab = null;`
- `public int instanceNum = 0;`
- `public enum Mode { Set, Add, AddNormalize };`
- `public Mode mode = Mode.Add;`
- `public float opacity = 1;`

## Map Set / Blur — Blur200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:360`.
Atributos: `menu="Map Set", name ="Blur", iconName="GeneratorIcons/Blur", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Blur"`.

- `[Val("Downsample")] public float downsample = 10f;`
- `[Val("Blur")] public float blur = 3f;`

## Map Set / Cavity — Cavity200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:394`.
Atributos: `menu="Map Set", name ="Cavity", iconName="GeneratorIcons/Cavity", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Cavity"`.

- `[Val("Type")]		public MatrixGenerators.Cavity200.CavityType type = MatrixGenerators.Cavity200.CavityType.Convex;`
- `[Val("Intensity")]	public float intensity = 3;`
- `[Val("Spread")]		public float spread = 10;`

## Map Set / Combine — Combine210

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsSpecial.cs:264`.
Atributos: `menu = "Map Set", name = "Combine", colorType = typeof(MatrixSet), iconName="GeneratorIcons/MapSetCombine", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixSetGenerators/Combine"`.

- `public Generator Gen { get; private set; }`
- `public ulong id;`
- `public ulong Id { get{return id;} set{id=value;} }`
- `public ulong LinkedOutletId { get; set; }`
- `public ulong LinkedGenId { get; set; }`
- `public Layer[] layers = new Layer[0];`
- `public Layer[] Layers => layers;`
- `public bool normalize = false;`

## Map Set / Contrast — Contrast200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:81`.
Atributos: `menu="Map Set", name ="Contrast", iconName="GeneratorIcons/Contrast", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Contrast"`.

- `[Val(name="Intensity")] public float brightness = 0f;`
- `[Val(name="Contrast")] public float contrast = 1f;`

## Map Set / Create — Create210

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsSpecial.cs:144`.
Atributos: `menu = "Map Set", name = "Create", colorType = typeof(MatrixSet), iconName="GeneratorIcons/MapSetCreate", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixSetGenerators/Create"`.

- `public PrototypeType prototypeType = PrototypeType.TerrainLayer;`
- `public TerrainLayer terrainLayer = null;`
- `public Texture2D texture = null;`
- `public GameObject prefab = null;`
- `public int instanceNum = 0;`
- `public float Opacity { get; set; }`
- `public Generator Gen { get; private set; }`
- `public ulong id;`
- `public ulong Id { get{return id;} set{id=value;} }`
- `public ulong LinkedOutletId { get; set; }`
- `public ulong LinkedGenId { get; set; }`
- `public Layer[] layers = new Layer[0];`
- `public Layer[] Layers => layers;`
- `public int guiExpanded;`
- `public enum Normalize { None, Sum, Layered }`
- `public Normalize normalize = Normalize.None;`

## Map Set / Curve — Curve200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:16`.
Atributos: `menu="Map Set", name ="Curve", iconName="GeneratorIcons/Curve", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Curve"`.

- `public Curve curve = new Curve( new Vector2(0,0), new Vector2(1,1) );`

## Map Set / Erosion — Erosion200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:524`.
Atributos: `menu="Map Set", name ="Erosion", iconName="GeneratorIcons/Erosion", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Erosion"`.

- `[Val("Iterations")]		 public int iterations = 3;`
- `[Val("Durability")] public float terrainDurability=0.9f;`
- `public float erosionAmount=1f;`
- `[Val("Sediment")]	 public float sedimentAmount=0.75f;`
- `[Val("Fluidity")] public int fluidityIterations=3;`
- `[Val("Relax")]		public float relax=0.0f;`
- `public float Complexity {get{ return iterations*2; }}`

## Map Set / Levels — Levels200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:45`.
Atributos: `menu="Map Set", name ="Levels", iconName="GeneratorIcons/Levels", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Levels"`.

- `public float inMin = 0;`
- `public float inMax = 1;`
- `public float gamma = 1f;`
- `public float outMin = 0;`
- `public float outMax = 1;`
- `public bool guiParams = false;`

## Map Set / Mask — Mask200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:159`.
Atributos: `menu="Map Set", name ="Mask", iconName="GeneratorIcons/MapMask", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Mask"`.

- `[Val("Invert")]	public bool invert = false;`

## Map Set / Parallax — Parallax210

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:561`.
Atributos: `menu = "Map Set", name = "Parallax", section=2, colorType = typeof(MatrixSet), iconName="GeneratorIcons/Parallax", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Parallax"`.

- `[Val("Offset")] public Vector2D offset;`
- `public enum Interpolation { None, Always, OnTransitions }`
- `[Val("Interpolation")] public Interpolation interpolation = Interpolation.Always;`

## Map Set / Pick — Pick210

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsSpecial.cs:86`.
Atributos: `menu = "Map Set", name = "Pick", colorType = typeof(MatrixSet), iconName="GeneratorIcons/MapSetPick", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixSetGenerators/Pick"`.

- `public PrototypeType prototypeType = PrototypeType.TerrainLayer;`
- `public TerrainLayer terrainLayer = null;`
- `public Texture2D texture = null;`
- `public GameObject prefab = null;`
- `public int instanceNum = 0;`
- `public bool createIfNotExists = true;`
- `public float opacity = 1;`

## Map Set / Selector — Selector200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:452`.
Atributos: `menu="Map Set", name ="Selector", iconName="GeneratorIcons/Selector", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Selector"`.

- `public enum RangeDet { Transition, MinMax}`
- `public RangeDet rangeDet = RangeDet.Transition;`
- `public enum Units { Map, World }`
- `public Units units = Units.Map;`
- `public Vector2 from = new Vector2(0.4f, 0.6f);`
- `public Vector2 to = new Vector2(1f, 1f);`

## Map Set / Slope — Slope200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:424`.
Atributos: `menu="Map Set", name ="Slope", iconName="GeneratorIcons/Slope", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Slope"`.

- `[Val("From")]			public float from = 30;`
- `[Val("To")]				public float to = 90;`
- `[Val("Smooth Range")]	public float range = 30f;`

## Map Set / Terrace — Terrace200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:486`.
Atributos: `menu="Map Set", name ="Terrace", iconName="GeneratorIcons/Terrace", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Terrace"`.

- `[Val("Seed")]		 public int seed = 12345;`
- `[Val("Num")]		 public int num = 10;`
- `[Val("Uniformity")] public float uniformity = 0.5f;`
- `[Val("Steepness")]	 public float steepness = 0.5f;`

## Map Set / Unity Curve — UnityCurve200

Fonte: `Assets/MapMagic/Generators/MatrixSets/Runtime/MatrixSetsGenerators.cs:111`.
Atributos: `menu="Map Set", name ="Unity Curve", iconName="GeneratorIcons/UnityCurve", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/UnityCurve"`.

- `public AnimationCurve curve = new AnimationCurve( new Keyframe[] { new Keyframe(0,0,1,1), new Keyframe(1,1,1,1) } );`
- `public Vector2 min = new Vector2(0,0);`
- `public Vector2 max = new Vector2(1,1);`

## Map/Initial / Constant — Constant200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixInitial.cs:16`.
Atributos: `menu="Map/Initial", name ="Constant", iconName="GeneratorIcons/Constant", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Constant"`.

- `[Val("Level", min:0)] public float level;`

## Map/Initial / Import — Import200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixInitial.cs:312`.
Atributos: `menu="Map/Initial", name ="Import", iconName="GeneratorIcons/Import", disengageable = true, disabled = false, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Import"`.

- `[Val("Map", priority = 10, type = typeof(MatrixAsset))]	public MatrixAsset matrixAsset;`
- `[Val("Wrap Mode", priority = 4)]	public CoordRect.TileMode wrapMode = CoordRect.TileMode.Clamp;`
- `[Val("Scale", priority = 3)]		public float scale = 1;`
- `[Val("Offset", priority = 2)]		public Vector2 offset;`
- `public bool hideDefaultToolGizmo { get; set; }`
- `public bool drawOffsetScaleGizmo = false;`

## Map/Initial / Noise — Noise200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixInitial.cs:32`.
Atributos: `menu="Map/Initial", name ="Noise", iconName="GeneratorIcons/Noise", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Noise"`.

- `public enum Type { Unity=0, Linear=1, Perlin=2, Simplex=3 };`
- `[Val("Type")] public Type type = Type.Perlin;`
- `[Val("Seed")]		public int seed = 12345;`
- `[Val("Intensity")]	public float intensity = 1f;`
- `[Val("Size")]		public float size = 200f;`
- `[Val("Detail")]		public float detail = 0.5f;`
- `[Val("Turbulence")] public float turbulence = 0f;`
- `[Val("Offset")]		public Vector2D offset = new Vector2D(0,0);`

## Map/Initial / Simple Form — SimpleForm200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixInitial.cs:183`.
Atributos: `menu="Map/Initial", name ="Simple Form", iconName="GeneratorIcons/SimpleForm", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/SimpleForm"`.

- `public enum FormType { GradientX, GradientZ, Pyramid, Cone }`
- `[Val("Type")]		public FormType type = FormType.Cone;`
- `[Val("Intensity")]	public float intensity = 1;`
- `[Val("Scale")]		public float scale = 1;`
- `[Val("Offset")]		public Vector2 offset;`
- `[Val("Wrap")]		public CoordRect.TileMode wrap = CoordRect.TileMode.Tile;`
- `public bool hideDefaultToolGizmo { get; set; }`
- `public bool drawOffsetScaleGizmo = false;`

## Map/Initial / Spot — Spot210

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixInitial.cs:462`.
Atributos: `menu = "Map/Initial", name = "Spot", section=2, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/Spot", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Spot"`.

- `[Val("Intensity")] public float intensity = 1;`
- `[Val("Position")] public Vector2D position;`
- `[Val("Radius")] public float radius = 30;`
- `[Val("Hardness")] public float hardness = 0.5f;`

## Map/Initial / Voronoi — Voronoi200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixInitial.cs:105`.
Atributos: `menu="Map/Initial", name ="Voronoi", iconName="GeneratorIcons/Voronoi", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Voronoi"`.

- `[Val("Intensity")]	public float intensity = 1f;`
- `[Val("Cell Size", min:0)]	public int cellSize = 50;`
- `[Val("Uniformity")]	public float uniformity = 0;`
- `[Val("Seed")]	public int seed = 12345;`
- `public enum BlendType { flat, closest, secondClosest, cellular, organic }`
- `[Val("Blend Type")]	public BlendType blendType = BlendType.cellular;`

## Map/Modifiers / Beach — Beach210

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:866`.
Atributos: `menu="Map/Modifiers", name ="Beach", iconName="GeneratorIcons/Beach", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Beach"`.

- `[Val("Level")]	public float level = 50;`
- `[Val("Contour Relax")]	public float relax = 100;`
- `[Val("Size")]	public float size = 40;`
- `[Val("Height")]	public float height = 5;`
- `[Val("Sand Tex Blur")]	public float sandBlur = 5f;`

## Map/Modifiers / Blend — Blend200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:246`.
Atributos: `menu="Map/Modifiers", name ="Blend", iconName="GeneratorIcons/Blend", disengageable = true, colorType = typeof(MatrixWorld), helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Blend"`.

- `public BlendAlgorithm algorithm = BlendAlgorithm.add;`
- `public float opacity = 1;`
- `public bool guiExpanded = false;`
- `public Layer[] layers = new Layer[] { new Layer(), new Layer() };`
- `public Layer[] Layers => layers;`
- `public enum BlendAlgorithm {`

## Map/Modifiers / Blur — Blur200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:406`.
Atributos: `menu="Map/Modifiers", name ="Blur", iconName="GeneratorIcons/Blur", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Blur"`.

- `[Val("Downsample")] public float downsample = 10f;`
- `[Val("Blur")] public float blur = 3f;`

## Map/Modifiers / Cavity — Cavity200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:435`.
Atributos: `menu="Map/Modifiers", name ="Cavity", iconName="GeneratorIcons/Cavity", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Cavity"`.

- `public enum CavityType { Convex=0, Concave=1, Both=2 }`
- `[Val("Type")]		public CavityType type = CavityType.Convex;`
- `[Val("Intensity")]	public float intensity = 3;`
- `[Val("Spread")]		public float spread = 10;`

## Map/Modifiers / Contrast — Contrast200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:140`.
Atributos: `menu="Map/Modifiers", name ="Contrast", iconName="GeneratorIcons/Contrast", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Contrast"`.

- `[Val(name="Intensity")] public float brightness = 0f;`
- `[Val(name="Contrast")] public float contrast = 1f;`

## Map/Modifiers / Curve — Curve200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:17`.
Atributos: `menu="Map/Modifiers", name ="Curve", iconName="GeneratorIcons/Curve", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Curve"`.

- `public Curve curve = new Curve( new Vector2(0,0), new Vector2(1,1) );`
- `[NonSerialized] public float[] histogram = null;`

## Map/Modifiers / Direction — Direction210

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:657`.
Atributos: `menu="Map/Modifiers", name ="Direction", iconName="GeneratorIcons/Direction", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Direction"`.

- `[Val("Hor Angle", min=-360, max=360)] public float horAngle = 0;`
- `[Val("Vert Angle", min=-89.99f, max=89.99f)] public float vertAngle = 0;`
- `[Val("Intensity", min =0)] public float intensity = 1;`
- `[Val("Wrapping", min=-1, max=1)] public float wrapping = 0;`

## Map/Modifiers / Erosion — Erosion200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:1034`.
Atributos: `menu="Map/Modifiers", name ="Erosion", iconName="GeneratorIcons/Erosion", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Erision"`.

- `[Val("Iterations")]		 public int iterations = 3;`
- `[Val("Durability")] public float terrainDurability=0.9f;`
- `public float erosionAmount=1f;`
- `[Val("Sediment")]	 public float sedimentAmount=0.75f;`
- `[Val("Fluidity")]	public int fluidityIterations=3;`
- `[Val("Relax")]		public float relax=0.0f;`
- `public float Complexity {get{ return iterations*2; }}`

## Map/Modifiers / Ledge — Ledge210

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:691`.
Atributos: `menu="Map/Modifiers", name ="Ledge", iconName="GeneratorIcons/Ledge", disengageable = true, advancedOptions = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Ledge"`.

- `[Val("Level")]		public float level = 50;`
- `[Val("Contour Blur")]	public float contourBlur = 3;`
- `[Val("Height")]		public float height = 10;`
- `[Val("Steep")]		public float steep = 30;`
- `[Val("Top Shoulder", "Advanced")]	public float topShoulder = 2f;`
- `[Val("Bottom Shoulder", "Advanced")]	public float bottomShoulder = 2f;`
- `[Val("Smooth", "Advanced")]	public bool smooth = true;`

## Map/Modifiers / Levels — Levels200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:97`.
Atributos: `menu="Map/Modifiers", name ="Levels", iconName="GeneratorIcons/Levels", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Levels"`.

- `public float inMin = 0;`
- `public float inMax = 1;`
- `public float gamma = 1f;`
- `public float outMin = 0;`
- `public float outMax = 1;`
- `[NonSerialized] public float[] histogram = null;`
- `public bool guiParams = false;`

## Map/Modifiers / Mask — Mask200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:212`.
Atributos: `menu="Map/Modifiers", name ="Mask", iconName="GeneratorIcons/MapMask", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/MapMask"`.

- `[Val("Invert")]	public bool invert = false;`

## Map/Modifiers / Normalize — Normalize200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:326`.
Atributos: `menu="Map/Modifiers", name ="Normalize", disengageable = true, iconName="GeneratorIcons/Normalize", drawInlets = false, drawOutlet = false, colorType = typeof(MatrixWorld), helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Normalize"`.

- `public float Opacity { get; set; }`
- `public Generator Gen { get; private set; }`
- `public ulong id;`
- `public ulong Id { get{return id;} set{id=value;} }`
- `public ulong LinkedOutletId { get; set; }`
- `public ulong LinkedGenId { get; set; }`
- `public NormalizeLayer[] layers = new NormalizeLayer[0];`
- `public NormalizeLayer[] Layers => layers;`

## Map/Modifiers / Parallax — Parallax210

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:1199`.
Atributos: `menu = "Map/Modifiers", name = "Parallax", section=2, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/Parallax", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Parallax"`.

- `[Val("Offset")] public Vector2D offset;`
- `public enum Interpolation { None, Always, OnTransitions }`
- `[Val("Interpolation")] public Interpolation interpolation = Interpolation.Always;`

## Map/Modifiers / Sediment — Sediment210

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:1132`.
Atributos: `menu="Map/Modifiers", name ="Sediment", iconName="GeneratorIcons/Sediment", disengageable = true, colorType = typeof(MatrixWorld), helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Sediment"`.

- `[Val("Cliff")]	public float cliffIntensity = 1f;`
- `[Val("Sediment")] public float sedimentIntensity = 1f;`

## Map/Modifiers / Selector — Selector200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:564`.
Atributos: `menu="Map/Modifiers", name ="Selector", iconName="GeneratorIcons/Selector", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Selector"`.

- `public enum RangeDet { Transition, MinMax}`
- `public RangeDet rangeDet = RangeDet.Transition;`
- `public enum Units { Map, World }`
- `public Units units = Units.Map;`
- `public Vector2 from = new Vector2(0.4f, 0.6f);`
- `public Vector2 to = new Vector2(1f, 1f);`

## Map/Modifiers / Slope — Slope200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:501`.
Atributos: `menu="Map/Modifiers", name ="Slope", iconName="GeneratorIcons/Slope", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Slope"`.

- `[Val("From")]			public float from = 30;`
- `[Val("To")]				public float to = 90;`
- `[Val("Smooth Range")]	public float range = 30f;`

## Map/Modifiers / Terrace — Terrace200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:603`.
Atributos: `menu="Map/Modifiers", name ="Terrace", iconName="GeneratorIcons/Terrace", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Terrace"`.

- `[Val("Seed")]		 public int seed = 12345;`
- `[Val("Num")]		 public int num = 10;`
- `[Val("Uniformity")] public float uniformity = 0.5f;`
- `[Val("Steepness")]	 public float steepness = 0.5f;`

## Map/Modifiers / Unity Curve — UnityCurve200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/MatrixModifiers.cs:173`.
Atributos: `menu="Map/Modifiers", name ="Unity Curve", iconName="GeneratorIcons/UnityCurve", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/UnityCurve"`.

- `public AnimationCurve curve = new AnimationCurve( new Keyframe[] { new Keyframe(0,0,1,1), new Keyframe(1,1,1,1) } );`
- `public Vector2 min = new Vector2(0,0);`
- `public Vector2 max = new Vector2(1,1);`

## Map/Output / CTS — CTSOutput200

Fonte: `Assets/MapMagic/Compatibility/CTS/CTSOutput.cs:16`.
Atributos: `menu = "Map/Output", name = "CTS", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `public string[] guiTextureNames = null;`
- `public CTSProfile profile;`
- `public Color[][] textureColors;`

## Map/Output / Custom Material — CustomShaderOutput200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/TexturesOut.cs:282`.
Atributos: `menu = "Map/Output", name = "Custom Material", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `public Color[][] textureColors;`
- `public string[] textureNames;`
- `public string[] altTextureNames= null;`
- `public TextureFormat textureFormat;`
- `public float textureBaseMapDistance;`

## Map/Output / Direct Textures — DirectTexturesOutput200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/TexturesOut.cs:563`.
Atributos: `menu = "Map/Output", name = "Direct Textures", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `public Color[][] textureColors;`
- `public string[] textureNames;`
- `public TextureFormat textureFormat;`

## Map/Output / DirectMatrices — DirectMatricesOutput200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/TexturesOut.cs:744`.
Atributos: `menu = "Map/Output", name = "DirectMatrices", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `public DictionaryOrdered<string,MatrixWorld> dict;`

## Map/Output / Grass — GrassOutput200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/GrassOut.cs:15`.
Atributos: `menu = "Map/Output", name = "Grass", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/GrassOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Grass"`.

- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public float density = 0.5f;`
- `public DetailPrototype prototype = new DetailPrototype() { dryColor = new Color(0.95f, 1f, 0.65f), healthyColor = new Color(0.5f, 0.65f, 0.35f) };`
- `public enum GrassRenderMode { Grass, Billboard, MeshVertexLit, MeshUnlit };`
- `public GrassRenderMode renderMode;`
- `public int[][,] detailLayers;`
- `public DetailPrototype[] detailPrototypes;`
- `public int patchResolution = 16;`
- `public int Resolution => detailLayers.Length != 0 ? detailLayers[0].GetLength(0) : 0;`

## Map/Output / Height — HeightOutput200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/HeightOut.cs:24`.
Atributos: `menu = "Map/Output", name = "Height", section=2, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/HeightOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Height"`.

- `public OutputLevel outputLevel = OutputLevel.Draft | OutputLevel.Main;`
- `public enum Interpolation { None, Smooth, Scale2X, Scale4X };`
- `public enum ApplyType { SetHeights, SetHeightsDelayLOD, TextureToHeightmap }`
- `public bool guiApplyType = false;`
- `public float[,] heights2D;`
- `public float height;`
- `public Coord offset;`
- `public int Resolution {get{ return heights2D.GetLength(0); }}`
- `public float[][,] heights2DSplits;`
- `public float height;`
- `public int res;`
- `public int margins;`
- `public int splitSize;`
- `public float height;`
- `public byte[] texBytes;`
- `public int Resolution {get{ return (int)Mathf.Sqrt(texBytes.Length/4); }}`

## Map/Output / MegaSplat — MegaSplatOutput200

Fonte: `Assets/MapMagic/Compatibility/MegaSplat/MegaSplatOutput.cs:15`.
Atributos: `menu = "Map/Output", name = "MegaSplat", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.


## Map/Output / MicroSplat — MicroSplatOutput200

Fonte: `Assets/MapMagic/Compatibility/MicroSplat/MicroSplatOutput.cs:19`.
Atributos: `menu = "Map/Output", name = "MicroSplat", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `[NonSerialized] public TerrainLayer prototype = null;`
- `public bool assignComponent;`
- `public MicroSplatPropData propData;`
- `public bool assignComponent;`
- `public MicroSplatPropData propData;`
- `public Material materialTemplate;`
- `public Color[][] colors;`

## Map/Output / MicroSplat — MicroSplatOutput200

Fonte: `Assets/MapMagic/Compatibility/MicroSplat/Placeholders/MicroSplatOutputPlaceholder.cs:14`.
Atributos: `menu = "Map/Output", name = "MicroSplat", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `[NonSerialized] public TerrainLayer prototype = null;`

## Map/Output / RTP — RTPOutput200

Fonte: `Assets/MapMagic/Compatibility/RTP/RTPOutput.cs:13`.
Atributos: `menu = "Map/Output", name = "RTP", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName = "GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `public ReliefTerrain rtp = null;`

## Map/Output / Textures — TexturesOutput200

Fonte: `Assets/MapMagic/Generators/Matrix/Runtime/TexturesOut.cs:105`.
Atributos: `menu = "Map/Output", name = "Textures", section =2, drawButtons = false, colorType = typeof(MatrixWorld), iconName="GeneratorIcons/TexturesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Textures"`.

- `[Val("Layer", cat:"Layer")] public TerrainLayer prototype;`
- `public Color color = new Color(0.75f, 0.75f, 0.75f, 1);`
- `public bool guiProperties;`
- `public bool guiRemapping;`
- `public bool guiTileSettings;`
- `[SerializeField] public int guiExpanded;`
- `public float[,,] splats;`
- `public TerrainLayer[] prototypes;`

## Map/Output / VS Pro Maps — VSProMapsOut

Fonte: `Assets/MapMagic/Compatibility/VegetationStudio/Runtime/VSProMapsOut.cs:24`.
Atributos: `menu = "Map/Output", name = "VS Pro Maps", section =2, drawButtons = false, colorType = typeof(MatrixWorld), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Grass"`.

- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public float density = 0.5f;`
- `public int maskGroup = 0;`
- `public int textureChannel = 0;`
- `public VegetationSystemPro srcSystem;`
- `public VegetationPackagePro package;`
- `public Color[][] colors;`
- `public int[] maskGroupNums;`
- `public bool copyVS;`

## Map/Portals / Enter — MatrixPortalEnter

Fonte: `Assets/MapMagic/Nodes/Portals.cs:12`.
Atributos: `menu = "Map/Portals", name = "Enter", iconName = "GeneratorIcons/PortalIn", lookLikePortal=true`.


## Map/Portals / Exit — MatrixPortalExit

Fonte: `Assets/MapMagic/Nodes/Portals.cs:13`.
Atributos: `menu = "Map/Portals", name ="Exit", iconName = "GeneratorIcons/PortalOut", lookLikePortal=true`.


## Objects/Initial / Get by Tag — GetByTag211

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsInitial.cs:179`.
Atributos: `menu = "Objects/Initial", name = "Get by Tag", iconName = "GeneratorIcons/Position", disengageable = true, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/GetByTag"`.

- `public string tag;`
- `[Val("Add.Margin", "Advanced")] public float additionalMargins = 0;`

## Objects/Initial / Positions — Positions200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsInitial.cs:14`.
Atributos: `menu="Objects/Initial", name ="Positions", iconName="GeneratorIcons/Position", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Scatter"`.

- `public Vector3[] positions= new Vector3[1];`

## Objects/Initial / Random — Random207

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsInitial.cs:92`.
Atributos: `menu="Objects/Initial", name ="Random", iconName="GeneratorIcons/Random", disengageable = true, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Scatter"`.

- `[Val("Seed")]			public int seed = 12345;`
- `[Val("Density")]		public float density = 10;`
- `[Val("Uniformity")]		public float uniformity = 0.1f;`

## Objects/Initial / Scatter — Scatter200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsInitial.cs:37`.
Atributos: `menu="Objects/Initial", name ="Scatter", iconName="GeneratorIcons/Scatter", disengageable = true, advancedOptions = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Scatter"`.

- `[Val("Seed")]			public int seed = 12345;`
- `[Val("Density")]		public float density = 10;`
- `[Val("Uniformity")]		public float uniformity = 0.1f;`
- `[Val("Relax", "Advanced")]	public float relax = 0.5f;`
- `[Val("Add.Margin", "Advanced")]	public float additionalMargins = 0;`

## Objects/Modifiers / Adjust — Adjust200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:14`.
Atributos: `menu="Objects/Modifiers", name ="Adjust", iconName="GeneratorIcons/Adjust", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Adjust"`.

- `public bool useRandom = false;`
- `public int seed = 12345;`
- `public float sizeFactor = 0;`
- `public enum Relativeness { absolute, relative };`
- `public Relativeness relativeness = Relativeness.relative;`
- `public Vector2 height = Vector2.zero;`
- `public Vector2 rotation = Vector2.zero;`
- `public Vector2 scale = Vector2.one;`

## Objects/Modifiers / Combine — Combine200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:535`.
Atributos: `menu="Objects/Modifiers", name ="Combine", iconName="GeneratorIcons/Combine", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Combine"`.

- `public Inlet<TransitionsList>[] inlets = new Inlet<TransitionsList>[2] { new Inlet<TransitionsList>(), new Inlet<TransitionsList>() };`

## Objects/Modifiers / Flatten — Flatten200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:725`.
Atributos: `menu="Objects/Modifiers", name ="Flatten", iconName="GeneratorIcons/Flatten", disengageable = true, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Flatten"`.

- `[Val("Radius")]			public float radius = 1;`
- `[Val("Hardness")]		public float hardness = 0.5f;`
- `[Val("Size Factor")]	public float sizeFactor = 0;`
- `public bool noiseFallof = false;`
- `public float noiseAmount = 1f;`
- `public float noiseSize = 10;`

## Objects/Modifiers / Floor — Floor200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:262`.
Atributos: `menu="Objects/Modifiers", name ="Floor", iconName=null, disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Clean_Up"`.

- `public enum Relativity{ absolute, relative };`
- `[Val("Relativity")] public Relativity relativity = Relativity.absolute;`

## Objects/Modifiers / Forest — Forest200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:1039`.
Atributos: `menu="Objects/Modifiers", name ="Forest", iconName="GeneratorIcons/Forest", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Forest"`.

- `[Val("Years")]			public int years = 100;`
- `[Val("Density")]		public float density = 10000;`
- `[Val("Fecundity")]		public float fecundity = 0.5f;`
- `[Val("Seed Dist")]		public float seedDist = 15;`
- `[Val("Reproductive Age")]public float reproductiveAge = 10;`
- `[Val("Survival Rate")]	public float survivalRate = 0.95f;`
- `[Val("Life Age")]		public float lifeAge = 100;`
- `[Val("Size Is Age")]	public bool sizeIsLife = true;`
- `[Val("Seed")]			public int seed = 12345;`

## Objects/Modifiers / Lerp — Lerp210

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:203`.
Atributos: `menu="Objects/Modifiers", name ="Lerp", iconName="GeneratorIcons/ObjectsMask", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Clean_Up"`.


## Objects/Modifiers / Mask — Mask200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:147`.
Atributos: `menu="Objects/Modifiers", name ="Mask", iconName="GeneratorIcons/ObjectsMask", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Clean_Up"`.

- `[Val("Seed")]	public int seed = 12345;`
- `[Val("Invert")]	public bool invert = false;`

## Objects/Modifiers / Move — Move210

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:1279`.
Atributos: `menu = "Objects/Modifiers", name = "Move", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/Move", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Move"`.

- `[Val("Direction")] public Vector2D direction;`

## Objects/Modifiers / Randomize — Randomize211

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:119`.
Atributos: `menu = "Objects/Modifiers", name = "Randomize", iconName = "GeneratorIcons/Random", colorType = typeof(TransitionsList), disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Split"`.

- `[Val("Seed")] public int seed = 12345;`

## Objects/Modifiers / Rarefy — Rarefy200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:447`.
Atributos: `menu="Objects/Modifiers", name ="Rarefy", iconName="GeneratorIcons/Rarefy", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Rerefy"`.

- `public Inlet<TransitionsList> inlet;`
- `public ulong id;`
- `[Val("Dist")] public float distance;`
- `[Val("SF")] public float sizeFactor;`
- `public Layer[] layers = new Layer[0];`
- `public bool self = true;`
- `public float distance = 1;`
- `public float sizeFactor = 0;`

## Objects/Modifiers / Safe Borders — SafeBorders218

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:1361`.
Atributos: `menu = "Objects/Modifiers", name = "Safe Borders", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/SafeBorders", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/ShrinkScale"`.

- `[Val("Edge Dist")] public float edgeDist = 10f;`

## Objects/Modifiers / Shrink Scale — ShrinkScale210

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:1324`.
Atributos: `menu = "Objects/Modifiers", name = "Shrink Scale", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/ShrinkScale", helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/ShrinkScale"`.

- `[Val("Multiply")] public float multiply = 0.001f;`
- `[Val("Invert")] public bool invert;`

## Objects/Modifiers / Slide — SlideGenerator200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:1195`.
Atributos: `menu="Objects/Modifiers", name ="Slide", iconName="GeneratorIcons/Slide", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Slide"`.

- `[Val("Blur")]		public int smooth = 2;`
- `[Val("Iterations")]	public int iterations = 100;`
- `[Val("Move Factor")]	public float moveFactor = 0.2f;`
- `[Val("Stop Slope")]	public float stopSlope = 10;`

## Objects/Modifiers / Split — Split200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:315`.
Atributos: `menu="Objects/Modifiers", name ="Split", iconName="GeneratorIcons/Split", colorType = typeof(TransitionsList), disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Split"`.

- `public string name = "Object Layer";`
- `public bool heightConditionActive = false;`
- `public Vector2 heightCondition = new Vector2(0,1);`
- `public bool rotationConditionActive = false;`
- `public Vector2 rotationCondition = new Vector2(0,360);`
- `public bool scaleConditionActive = false;`
- `public Vector2 scaleCondition = new Vector2(0,100);`
- `public float chance = 1;`
- `public Generator Gen { get; private set; }`
- `public ulong id;`
- `public ulong Id { get{return id;} set{id=value;} }`
- `public SplitLayer[] layers = new SplitLayer[0];`
- `public IList<IUnit> Layers { get => layers; set {return;} }`
- `public bool Inversed => true;`
- `public bool HideFirst => false;`
- `public int guiExpanded = -1;`
- `public enum MatchType { layered, random };`
- `[Val("Match")]	public MatchType matchType = MatchType.random;`
- `[Val("Seed")]	public int seed = 12345;`

## Objects/Modifiers / Spread — Spread200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:567`.
Atributos: `menu="Objects/Modifiers", name ="Spread", iconName="GeneratorIcons/Spread", disengageable = true, helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Propagate"`.

- `public bool retainOriginals = true;`
- `public int seed = 12345;`
- `public UnityEngine.Vector2 growth = new UnityEngine.Vector2(2,3);`
- `public UnityEngine.Vector2 distance = new UnityEngine.Vector2(1,2);`
- `public float sizeFactor = 0;`

## Objects/Modifiers / Stamp — Stamp200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:898`.
Atributos: `menu="Objects/Modifiers", name ="Stamp", iconName="GeneratorIcons/Stamp", disengageable = true, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Stamp"`.

- `[Val("Size", "Custom")]	public float size = 1;`
- `[Val("Intensity", "Custom")]	public float intensity = 1;`
- `[Val("Hardness", "UseFallof")]	public float hardness = 0.8f;`
- `[Val("Size Factor", "Custom")]	public float sizeFactor = 1;`
- `[Val("Intensity Factor", "Custom")]	public float intensityFactor = 1;`
- `public enum BlendType { Max, Add }`
- `public BlendType blendType = BlendType.Max;`
- `[Val("Use Rotation", "Custom", isLeft = true)]	public bool useRotation = true;`
- `[Val("Use Falloff", "Custom", isLeft = true)]	public bool useFalloff = false;`

## Objects/Modifiers / Stroke — Stroke200

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsModifiers.cs:817`.
Atributos: `menu="Objects/Modifiers", name ="Stroke", iconName="GeneratorIcons/Flatten", disengageable = true, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/object_generators/Stroke"`.

- `[Val("Radius")]			public float radius = 1;`
- `[Val("Hardness")]		public float hardness = 0.5f;`
- `[Val("Size Factor")]	public float sizeFactor = 0;`
- `public bool noiseFallof = false;`
- `public float noiseAmount = 10f;`
- `public float noiseSize = 5f;`

## Objects/Outputs / Objects — ObjectsOutput

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsOut.cs:47`.
Atributos: `menu = "Objects/Outputs", name = "Objects", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/ObjectsOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Objects"`.

- `public GameObject[] prefabs = new GameObject[1];`
- `public PositioningSettings posSettings = null;`
- `public BiomeBlend biomeBlend = BiomeBlend.Random;`
- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public bool guiMultiprefab;`
- `public bool guiProperties;`
- `public bool allowReposition = true;`
- `public bool instantiateClones = false;`
- `public int seed = 12345;`
- `public bool objHeight = true;`
- `public bool relativeHeight = true;`
- `public bool guiHeight;`
- `public bool useRotation = true;`
- `public bool takeTerrainNormal = false;`
- `public bool rotateYonly = false;`
- `public bool regardPrefabRotation = false;`
- `public bool guiRotation;`
- `public bool useScale = true;`
- `public bool scaleYonly = false;`
- `public bool regardPrefabScale = false;`
- `public bool guiScale;`
- `public ObjectsPool.Prototype[] prototypes;`
- `public List<Transition>[] transitions;`
- `public float terrainHeight;`
- `public int objsPerIteration=500;`
- `public int Resolution {get{ return 0; }}`

## Objects/Outputs / Trees — TreesOutput

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsOut.cs:245`.
Atributos: `menu = "Objects/Outputs", name = "Trees", section=2, colorType = typeof(TransitionsList), iconName="GeneratorIcons/TreesOut", helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Objects"`.

- `public GameObject[] prefabs = new GameObject[1];`
- `public PositioningSettings posSettings = null;`
- `public BiomeBlend biomeBlend = BiomeBlend.Random;`
- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public bool guiMultiprefab;`
- `public bool guiProperties;`
- `public int seed = 12345;`
- `public Color color = Color.white;`
- `public Color lightmapColor = Color.white;`
- `public float bendFactor;`
- `public bool objHeight = true;`
- `public bool relativeHeight = true;`
- `public bool guiHeight;`
- `public bool useRotation = true;`
- `public bool takeTerrainNormal = false;`
- `public bool rotateYonly = false;`
- `public bool regardPrefabRotation = false;`
- `public bool guiRotation;`
- `public bool useScale = true;`
- `public bool scaleYonly = false;`
- `public bool regardPrefabScale = false;`
- `public bool guiScale;`
- `public TreeInstance[] treeInstances;`
- `public TreePrototype[] treePrototypes;`
- `public int Resolution {get{ return 0; }}`

## Objects/Outputs / VS Pro Objs — VSProObjectsOut

Fonte: `Assets/MapMagic/Compatibility/VegetationStudioObjects/Runtime/VSProObjectsOut.cs:24`.
Atributos: `menu = "Objects/Outputs", name = "VS Pro Objs", section =2, drawButtons = false, colorType = typeof(TransitionsList), helpLink = "https://gitlab.com/denispahunov/mapmagic/wikis/output_generators/Grass"`.

- `public OutputLevel outputLevel = OutputLevel.Main;`
- `public PositioningSettings posSettings = null;`
- `public BiomeBlend biomeBlend = BiomeBlend.Random;`
- `public string id;`
- `public string lastUsedName;`
- `public string lastUsedType;`
- `public Layer[] layers = new Layer[] { new Layer() };`
- `public bool objHeight = true;`
- `public bool relativeHeight = true;`
- `public bool guiHeight;`
- `public bool useRotation = true;`
- `public bool takeTerrainNormal = false;`
- `public bool rotateYonly = false;`
- `public bool regardPrefabRotation = false;`
- `public bool guiRotation;`
- `public bool useScale = true;`
- `public bool scaleYonly = false;`
- `public bool regardPrefabScale = false;`
- `public bool guiScale;`
- `public VegetationSystemPro srcSystem;`
- `public VegetationPackagePro package;`
- `public bool copyVS;`
- `public List<Transition>[] trns;`
- `public string[] ids;`
- `public int Resolution {get{ return 0; }}`

## Objects/Portals / Enter — ObjectsPortalEnter

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsPortals.cs:12`.
Atributos: `menu = "Objects/Portals", name = "Enter", iconName = "GeneratorIcons/PortalIn", lookLikePortal=true`.


## Objects/Portals / Exit — ObjectsPortalExit

Fonte: `Assets/MapMagic/Generators/Objects/Runtime/ObjectsPortals.cs:13`.
Atributos: `menu = "Objects/Portals", name ="Exit", iconName = "GeneratorIcons/PortalOut", lookLikePortal=true`.


## Spline/Portals / Enter — SplinePortalEnter

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesPortals.cs:12`.
Atributos: `menu = "Spline/Portals", name = "Enter", iconName = "GeneratorIcons/PortalIn", lookLikePortal=true`.


## Spline/Portals / Exit — SplinePortalExit

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesPortals.cs:15`.
Atributos: `menu = "Spline/Portals", name ="Exit", iconName = "GeneratorIcons/PortalOut", lookLikePortal=true`.


## Spline/Standard / Align — Align200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:398`.
Atributos: `menu="Spline/Standard", name ="Align", iconName="GeneratorIcons/Constant", colorType = typeof(SplineSys), disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Range")] public float range = 30;`
- `[Val("Flat")] public float flat = 0.25f;`
- `[Val("Detail")] public float detail = 0f;`

## Spline/Standard / Avoid — Avoid200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:795`.
Atributos: `menu="Spline/Standard", name ="Avoid", iconName=null, disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Distance")] public float distance = 10;`
- `[Val("Size Factor")] public int sizeFactor = 0;`
- `[Val("Iterations")] public int iterations = 10;`

## Spline/Standard / Combine — Combine217

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:664`.
Atributos: `menu="Spline/Standard", name ="Combine", iconName="GeneratorIcons/Blend", disengageable = true, colorType = typeof(SplineSys), helpLink = "https://gitlab.com/denispahunov/mapmagic/-/wikis/MatrixGenerators/Blend"`.

- `public Layer[] layers = new Layer[] { new Layer(), new Layer() };`
- `public Layer[] Layers => layers;`

## Spline/Standard / Floor — Floor200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:740`.
Atributos: `menu="Spline/Standard", name ="Floor", iconName=null, disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.


## Spline/Standard / Interlink — Interlink200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:43`.
Atributos: `menu="Spline/Standard", name ="Interlink", iconName="GeneratorIcons/Constant", colorType = typeof(SplineSys), disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Iterations")]		public int iterations = 8;`
- `[Val("Max Links")]		public int maxLinks = 4;`
- `[Val("Within Tile")]	public bool withinTile = true;`
- `public enum Clamp { Off, Full, Active }`
- `[Val("Clamp")]	public Clamp clamp;`

## Spline/Standard / Isoline — Isoline200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:885`.
Atributos: `menu="Spline/Standard", name ="Isoline", iconName=null, disengageable = true, colorType = typeof(SplineSys), helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.


## Spline/Standard / Manual — Manual210

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:17`.
Atributos: `menu="Spline/Standard", name ="Manual", iconName="GeneratorIcons/Constant", colorType = typeof(SplineSys), disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `public Vector3[] positions = new Vector3[1];`

## Spline/Standard / Optimize — Optimize200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:612`.
Atributos: `menu="Spline/Standard", name ="Optimize", iconName=null, disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Split")] public int split = 3;`
- `[Val("Deviation")] public float deviation = 1;`

## Spline/Standard / Pathfinding — Pathfinding200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:196`.
Atributos: `menu="Spline/Standard", name ="Pathfinding", iconName=null, disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Resolution")]			public int resolution = 32;`
- `[Val("Distance Factor")]		public float distanceFactor = 1f;`
- `[Val("Elevation Factor")]		public float elevationFactor = 1f;`
- `[Val("Straighten Factor")]		public float straightenFactor = 1f;`
- `[Val("Max Elevation")]		public float maxElevation = 0.1f;`
- `[Val("Max Iterations")]		public int maxIterations = 1000000;`

## Spline/Standard / Push — Push200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:837`.
Atributos: `menu="Spline/Standard", name ="Push", iconName=null, disengageable = true, colorType = typeof(SplineSys), helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Distance")] public float distance = 10;`
- `[Val("Size Factor")] public int sizeFactor = 0;`
- `[Val("Iterations")] public int iterations = 10;`

## Spline/Standard / Relax — Relax200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:638`.
Atributos: `menu="Spline/Standard", name ="Relax", iconName=null, disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Blur")] public float blur = 1;`
- `[Val("Iterations")] public int iterations = 3;`

## Spline/Standard / Silhouette — Silhouette200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:906`.
Atributos: `menu="Spline/Standard", name ="Silhouette", iconName=null, disengageable = true, colorType = typeof(SplineSys), helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Width")] public float width = 10;`

## Spline/Standard / Stamp — Stamp200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:500`.
Atributos: `menu="Spline/Standard", name ="Stamp", iconName="GeneratorIcons/Constant", colorType = typeof(SplineSys), disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `public enum Algorithm { Flatten, Detail, Both };`
- `public Algorithm algorithm;`
- `public float flatRange = 2;`
- `public float blendRange = 16;`
- `public float detailRange = 32;`
- `public int detail = 1;`
- `public float fallof = 1;`

## Spline/Standard / Stroke — Stroke200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:325`.
Atributos: `menu="Spline/Standard", name ="Stroke", iconName="GeneratorIcons/Constant", disengageable = true, colorType = typeof(SplineSys), helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Width")]	  public float width = 10;`
- `[Val("Hardness")] public float hardness = 0.0f;`

## Spline/Standard / Weld Close — WeldClose200

Fonte: `Assets/MapMagic/Generators/Splines/Runtime/SplinesGenerators.cs:715`.
Atributos: `menu="Spline/Standard", name ="Weld Close", iconName=null, disengageable = true, helpLink ="https://gitlab.com/denispahunov/mapmagic/wikis/map_generators/constant"`.

- `[Val("Threshold")] public float threshold = 25;`
