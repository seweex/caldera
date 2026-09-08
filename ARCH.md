## Caldera Architecture

The main class of the library is `RenderGraph`. It stores all data that belongs to the all render-graph-relatives managers. Also, there are some invocable objects that have no state and are used for transform the data.

#### Types of objects
- `Storages`: They:
	- Have an abstract state
	- Can be accessed by the user via its methods only
	- Reads by managers, not functors
	- Do no logic work
- `Managers`: They:
	- Have an abstract state
	- Are accessed by functors primarily
	- Are usually created once in a program
	- Do a lot of hard work
- `Functors`: They:
	- Have no state or a lightweight one
	- Usually have only one method
	- Hand the storage's data to the managers or touch it directly
	- Transform the data
	- Do a helping work (usually just delegates it or split to the steps)
- `Declarators`: They:
	- Have a specified state
	- Have a lot of methods
	- Accumulates descriptions of the future bug object
	- Are considered as smart-descriptors
- `Descriptors` are lightweight and often POD types. They describe the data itself and some input parameters

#### Facade objects:
- `Storages`:
	- Render Graph. It stores all stages in it.
	- Resource Map. It maps a virtual resource id to an actual vulkan resource handle
	- Semaphore Map. It provides semaphores to executor
- `Managers`:
	- Resource State Manager. It's a cross-graph state controller
	- Command Buffer Distributor. It provides command buffers to executor
- `Functors`:
	- Graph Compiler. It compiles the graph
	- Graph Allocator. It allocates graph memory and resources
	- Graph Executor. It executes a graph and returns a sumbittable data
- `Declarators`:
	- Graph Pass Declarator. It describes what a graph pass does
	- Graph Declarator. It describes what a graph does
- `Descriptors`:
	- Resource ID. It's an alias for a vulkan resource
	- Resource Building Reference. It represent a certain version of resource

#### Usage Assumption
- Create `Resource State Manager`
- Create and describe a `Graph Declaration`
- Compile a graph with it
- Allocate a graph with it
- Create and fill `Resource Map`, `Semaphore Map`, and `Command Buffer Distributor`
- Call a graph executor with:
	- Resource Map
	- Semaphore Map
	- Command Buffer Distributor
	- Resource State Manager
- Finally get `Submittable Graph` and submit it into queues