<h1 align="center">
  <a href="https://github.com/VTTI-CSM/CargoNetSim">
    <img src="https://github.com/user-attachments/assets/72c29fe8-cb40-4f61-bb4b-447cafff9c3a" alt="CargoNetSim"/>

  </a>
  <br/>
  CargoNetSim [Cargo Network Simulator]
</h1>

<p align="center">
<!--   <a href="http://dx.doi.org/10.1109/SM63044.2024.10733439">
    <img src="https://zenodo.org/badge/DOI/10.1109/SM63044.2024.10733439.svg" alt="DOI">
  </a> -->
  <a href="https://www.gnu.org/licenses/gpl-3.0">
    <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GNU GPL v3">
  </a>
  <a href="https://github.com/AhmedAredah/CargoNetSim/actions/workflows/ci.yml">
    <img src="https://github.com/AhmedAredah/CargoNetSim/actions/workflows/ci.yml/badge.svg" alt="CI">
  </a>
  <a href="https://github.com/VTTI-CSM/CargoNetSim/releases">
    <img alt="GitHub tag (latest by date)" src="https://img.shields.io/github/v/tag/VTTI-CSM/CargoNetSim.svg?label=latest">
  </a>
  <img alt="GitHub All Releases" src="https://img.shields.io/github/downloads/VTTI-CSM/CargoNetSim/total.svg">
  <a href="">
    <img src="https://img.shields.io/badge/CLA-CLA%20Required-red" alt="CLA Required">
    <a href="https://cla-assistant.io/VTTI-CSM/CargoNetSim"><img src="https://cla-assistant.io/readme/badge/VTTI-CSM/CargoNetSim" alt="CLA assistant" /></a>
  </a>
</p>

<div align="center">

<!-- ALL-CONTRIBUTORS-BADGE:START - Do not remove or modify this section -->
[![All Contributors](https://img.shields.io/badge/all_contributors-2-orange.svg?style=flat-square)](#contributors-)
<!-- ALL-CONTRIBUTORS-BADGE:END -->

</div>


<p align="center">
  <a href="https://github.com/VTTI-CSM/CargoNetSim/releases" target="_blank">Download CargoNetSim</a> |
  <a href="https://VTTI-CSM.github.io/CargoNetSim/" target="_blank">Documentation</a>
</p>

<p align="center">
  <a> For questions or feedback, contact <a href='mailto:AhmedAredah@vt.edu'>Ahmed Aredah</a> or <a href='mailto:HRakha@vtti.vt.edu'>Prof. Hesham Rakha<a>
</p>

# CargoNetSim

CargoNetSim is a multimodal freight operations optimization tool. This comprehensive software provides a graphical user interface for designing, simulating, and optimizing complex logistics networks across various transportation modes.

<!-- Add screenshot of GUI here -->
<p align="center">
  <img src="https://github.com/user-attachments/assets/15bfc540-adcd-46c3-8c1d-c42b5d99c5b3" alt="CargoNetSim GUI" width="800">
  <br>
  <em>CargoNetSim Graphical User Interface</em>
</p>

## Key Features

- **Interactive Network Design**: Intuitive visual interface for creating and modifying transportation networks
- **Multimodal Support**: Integrated simulation of maritime (ship), rail, and road (truck) transportation modes
- **Advanced Optimization**: Algorithms for route optimization, and cost minimization
- **Scenario Analysis**: Tools for comparing alternative network configurations and operational strategies

## System Requirements

### Supported Operating Systems
- Windows 10/11 (64-bit)
- Ubuntu 18.04 LTS or newer
- macOS 12 or newer

### Hardware Requirements
- 8GB RAM minimum (16GB recommended)
- 2.0+ GHz multi-core processor
- 1GB available disk space
- OpenGL 3.3 compatible graphics

### Prerequisites
- C++17 compatible compiler
- CMake 3.16 or later
- Qt6 (Core, Gui, Widgets, Network components)
- Container library
- RabbitMQ C client

## Installation

### Required Dependencies
- **KDReports**: Reporting functionality
- **TerminalSim**: [Available on GitHub](https://github.com/AhmedAredah/TerminalSim)
- **ShipNetSim Server**: [Available on GitHub](https://github.com/VTTI-CSM/ShipNetSim)
- **CargoNetSim Server**: [Available on GitHub](https://github.com/VTTI-CSM/CargoNetSim)
- **Integration Server**: [Available on GitHub](https://github.com/AhmedAredah/IntegrationTrucks)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/VTTI-CSM/CargoNetSim.git
cd CargoNetSim

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Run the application
./CargoNetSim  # Linux/macOS
.\CargoNetSim.exe  # Windows
```


## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/AhmedAredah"><img src="https://avatars.githubusercontent.com/u/77444744?v=4?s=100" width="100px;" alt="Ahmed Aredah"/><br /><sub><b>Ahmed Aredah</b></sub></a><br /><a href="https://github.com/VTTI-CSM/CargoNetSim/commits?author=ahmedaredah" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/heshamrakha"><img src="https://avatars.githubusercontent.com/u/11538915?v=4?s=100" width="100px;" alt="Hesham Rakha"/><br /><sub><b>Hesham Rakha</b></sub></a><br /><a href="#projectManagement-heshamrakha" title="Project Management">📆</a></td>
    </tr>
  </tbody>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->
