# p2g
Batch-style gcode generation tool for PCB manufacturing.

This uses gerbv to convert gerber/excelon files to bitmaps, and does all processing using opencv. This was easy, but is not memory efficient. Large boards may take several GB of RAM while processing.

# Building under debian-based linuxes

This will work on a raspberry pi or any linux computer.

```bash
apt-get install -y gerbv git libyaml-cpp-dev make g++ libopencv-dev libboost-system-dev libboost-filesystem-dev
git clone https://github.com/lhartmann/p2g.git
cd p2g
make
```

Once complete you will have a file named `p2g`, and a few sample config files under presets.

# Usage

Place you preset and gerbers on the same folder, then run the command below.

```bash
p2g preset.p2g
```

# Configuration files

Config files are broken into a few sections. These can be pretty overwhelming, so you should probably try out a few presets, enable debug, edit only the input section, and get a feel for it before going deep into creating your own.

## Global options

These include `ppmm` which sets the import resolution, and `debug` which enables a lot of intermediate png files to be dumped. Give debug a try if you want to better understand the steps of each process.

```yaml
# Gerber Import resolution in pixels/mm.
# A good rule of thumb is having 25 pixels for your smallest feature (tool or trace).
ppmm: 100
#margin: 4

# Generates debug images along the process.
# debug: true
```

## Export options

These are still experimental, and mostly intended for panelization.

```yaml
#export-options:
#  replicate:
#    rows: 3
#    cols: 1
#  rotate: 90
#  translate:
#    zero: lower-left
```

## Input section

`inputs` is used to select files to process, and creates associations between labels and actual files, which should reside in the same directory as the config file. Labels are arbitrary, and serve mostly to reduce the number of edits when reusing the preset with different files. Any file supported by gerbv can be used, but the must be at least one gerber so we can read the bounding box from.

```yaml
# These are the input layers to be used.
# Layer names don't matter, but must be unique.
# If any layer is not available, then jobs depending on it are simply skipped.
inputs:
  top:         top.gbr
  bottom:      bottom.gbr
  outline:     outline.gbr
  drill-npth:  drill-npth.drl
  drill-pth:   drill-pth.drl
  tabs:        tabs.gbr
  text-top:    text-top.gbr
  text-bottom: text-bottom.gbr
  slots:       slots.gbr
```

## Tools section

`tools` describes all tools you have in your library, even if you don´t use them later. Labels are again arbitrary, and only used to refer to the tools later. Options are all explained as comments.

```yaml
tools:
  defaultmill:                   # Template for other mills.
    description: Default mill
    type:        mill            # Mills can cut sideways
    diameter:    1.00            # Diameter (mm)
    depth:       0.10            # How deep the final cut should be?
    infeed:      0.75            # How deep each pass should be?
    speed:       12000           # Spindle speed, RPM
    feed:        120             # Horizontal feed (mm/min)
    plunge:      240             # Vertical feed while going down (mm/min)
    runs:        50              # Maximum number of contours (not passes)
    overlap:     0.1             # Fraction of a cut that should be cut again by the next tool path.

  defaultdrill:                  # Template for other drills.
    description: Default Drill
    type:        drill           # Drills can only drill
    diameter:    1.0
    depth:       2.1
    infeed:      2.1             # For pecking, set less than depth
    speed:       12000
    plunge:      240

  I0200:
    like:        defaultmill     # Inherit from default mill, then replace some properties.
    description: Isolation milling tool, d=0.200mm
    diameter:    0.200
# ...
  D0450:
    like:        defaultdrill
    description: Drill 0.45mm
    diameter:    0.45
    predrill:    I0254           # Fine drill, add a marking while running isolation.
# ...
```

## Jobs section

`jobs` controls what operations you want to perform, as well as what tools and files to be used for each. Job labels are arbitrary, and used later to export toolpaths. There are a few types of jobs available, each will generate a set of toolpaths.

### Isolate jobs

`isolate` jobs implement the basic milling procedure. All non-copper region will (may) be milled away using the tols provided. It requires a `copper` inputs for the traces, and `outline` inputs to set the desired milling region. If input `drills` are specified then tey are subtracted from the copper area, which may make usefull drill guides. Multiple `tools` may be specified to process the isolation. The first tool is called 'primary', is only used for a single contour. and it establishes the final copper surface, so it shuld be sharp. Middle tools are used for bulk removal, and will generate as many countours as can fit. You may set multiple bulk tools in descending order. The last tool, if smaller or equal diameter to the primary, is called 'detail', and is also allowed to touch final surfaces where previous tools could not reach.

```yaml
jobs:
  bottom:
    type: isolate                # "isolate" jobs will clear all the copper, unless stopped by the runs limit of the tools.
    inputs:                      # "isolate" requires "copper" and "outline" inputs.
      - copper:  bottom          # For copper, use "bottom" from input section.
      - outline: outline         # Copper removal is limited to the board outline + largest tool diameter.
      - drill:   drill-pth       # "drill" is optional, and removes copper from the holes.
      - drill:   drill-npth      # Repeated inputs get added together, usefull for joining plated and non-plated holes.
    tools:                       # List of tools to use on the job. Sort as: primary, BIG, medium, ..., small
      - I0254                    # Primary tool does a single contour around all traces.
      - I1000                    # Tools > primary are bulk, run to their limit, but won't touch trace surfaces.
      - I0254                    # Tools <= primary are detail, run to their limit, and may touch trace surfaces.
```

### Voronoi Jobs

`voronoi` jobs will isolate each pair of traces with a sinlge tool pass, as far as possible from both. This is very time-efficient on slow CNCs, causes less tool wear, as is more tolerant of inacurate cut width. May not be the easiest to solder as it cuts only once, and will leave extra copper that may interfere with high-voltage areas. It requires `copper` inputs for the traces, and `outline` inputs to set the cut boundaries. Optionally a `mask` can be supplied to limit voronoi cuts to within that region. If multiple tools are specified each path segment will use the largest tool that fits between the copper traces, which may help with high voltage areas.

```yaml
# The job section defines how the toolpaths are generated.
# Names don't matter, but must be unique.
jobs:
  bottom:
    type: voronoi                # Isolate only where needed, keeping tool farthest from traces.
    extend: 0.5                  # Extend 0.5mm beyond board outline
    inputs:                      # "voronoi" requires "copper" and "outline" inputs.
      - copper:  bottom
      - outline: outline
      - mask:    bottom          # Limit toolpaths to within 5mm of copper traces
        dilate:  5
    tools:
      - I0254                    # Muultiple tools are provided, each trace uses the largest one
      - I1000                    # that fits it's position.
```

## Drill Jobs

Used to processes drills and milled holes. Requires only `drills` as inputs, which can be repeatec to combine multiple files. Tools should be given is ascending diameter order, and the nearest matching area is used. If the last tool is a mill then it will be used for all holes larger than the largest drill.

```yaml
jobs:
  drill:
    type: drill                  # "drill" jobs also do mill-drills
    priority: 1000               # Priority added to toolpaths from this job. Lower numbers executed first.
    inputs:                      # Drill jobs only need a "drill" input
      - drill:   drill-pth
      - drill:   drill-npth      # But you can repeat to cobine multiple files.
    tools:                       # Tools to use, sorted by diameter.
      - D0450
      - D0700
      - D0800
      - D0900
      - D1000
      - M1000                    # You can add a mill last, for larger holes and slots
```

## Paint Jobs

Paint jobs use the same algorith as isolate jobs, except they operate on the positive gerber area, not in the space between traces, i.e., it mills what drawn on the gerber as a hole on the copper. This is also called a pocket on other programs. Paint requires only `area` inputs, and a list of tools following the same rules as isolate jobs. Common uses are fow debossed text on copper, orf for milling high voltage isolation slots.

```yaml
jobs:
  text-bottom:                   # Paint jobs fill areas. Usefull for text engraving.
    type: paint
    inputs:
      - area: text-bottom
    tools:
      - I0254

  slots:                         # HV isolation slots may also be created with "paint" jobs,
    type: paint
    priority: 1500               # After "drill", before "cutout".
    inputs:
      - area: slots
    tools:
      - M1000
```

## Cutout Jobs

Creates the outter and inner contour cuts, even if you have nested contours. For now tabs can't be auto generated. They must be drawn on a geber layer, as provided as an input.

```yaml
jobs:
  cutout:
    type: cutout                 # "cutout" jobs will do inside and outside cuts.
    priority: 2000               # After those from "drill" ansd "slots".
                                 # Outter contours will have incremented priority, so as to run
                                 # after their inner contours. This is why priorities were chosen
                                 # to be 1000,1500,2000, and not 1,2,3.
    inputs:                      # Only an "outline" input is required.
      - outline: outline         # Make sure the outline is closed. Nesting is OK.
      - tabs:    tabs            # 'tabs' are optional. Positive prevents cutout.
    tools:
      - M1000                    # Only a single tool is allowed, but it may require and
                                 # generate predrills as specified in the "tools" section.
```

## Alignment holes

Experimental, meant for double sided boards. Not well tested.

```yaml
  alignment_holes:
    type: alignment_holes
    inputs:
      - outline: outline
    tools:
      - D1000
```

# Output section

This section controls which output files will be created, their types, and what paths will be included on each. Three types are available: `hpgl` as used for Bungard CNC and several pen plotters, `gcode` as used by most CNCs, and `preview` for writing a png files. `side` specifies were the file will be machined from, and may be `top` with no mirroring, or `bottom` for mirroring as (X, -Y).

Each file takes a `paths` argument, which contains a list of filters for toolpath selection. Filters are evaluated for each toolpath, in the order defined by the list, and the first matching one is considered. Filter may select by job and/or tool labels, taking all if omitted. Filters may also add `exclude: true` to block certain jobs/tols.

```yaml
# The outputs section controls how to generate files from the toolpaths.
# Any number of files can be generated combining toolpaths from all jobs.
# In this example, one file is generated for each tool and side.
outputs:
  # Preview images are useful for inspection
  - file: 02-preview-bottom.png
    type: preview
    side: bottom
    paths:                               # Almost all are visible from the bottom, so...
      - { job: bottom      }
      - { job: bottom-bulk }
      - { job: text-bottom }
      - { job: drill    }
      - { job: slots    }
      - { job: cutout   }
      - { job: alignment_holes   }

  # Bottom-side, isolation:
  - file: CNC3018-10-bottom-mill-0.254mm.gcode
    side: bottom
    paths:
      - { job: bottom,      tool: I0254 }
      - { job: text-bottom, tool: I0254 }
      - { job: drill,       tool: I0254 }  # Fine tools may have predrills
```

# License

All rights reserved (to the extents allowed by github), code provided AS-IS, no warranties.

I intend to open this, but not yet sure which license is more suited.
