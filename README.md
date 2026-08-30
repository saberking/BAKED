# 

Sampler based on DPF + ImGui template plugin project with drag-and-drop (Windows/wine only at the moment)



The Drag and Drop code is in src/DragAndDrop.hpp. To use the header file it must be included in the PluginUI.cpp file and the plugin UI class must include ```new MyOleDropTarget(this);``` in its constructor. The plugin UI class must also implement the following functions:

```
void setDroppedFilePath(const char* path) override {
//do whatever you want with the path
}

Window& getWindow() const override {
    return UI::getWindow();//leave this alone
}
```
