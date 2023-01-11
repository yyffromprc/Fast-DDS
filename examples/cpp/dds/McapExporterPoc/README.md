# Dependencies

* MCAP: header only library already available in PoC folder
* [lz4](https://lz4.github.io/lz4/) (requiered)
* [zstd](https://facebook.github.io/zstd/) (requiered)

# Build instructions

```bash
source ~/fastdds/install/setup.zsh
cd ~/fastdds/src/fastrtps/examples/cpp/dds/McapExporterPoc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j8
touch output.mcap
```

# Run PoC

* Terminal

    ```bash
    ./McapExporterPoc
    ```

* Foxglove Studio

    * Open Foxglove Studio (desktop or web application)
    * *Open local file -> output.mcap*
    * Load ``resources/fastdds_poc_layout.json`` file as existing layout or click *Add panel -> Data Source Info / Table / Plot / Raw Messages* to create the layout from scratch.

