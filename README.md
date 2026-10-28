# OpenOffice — 开源 Word 文档编辑器（Qt 6）

一个基于 **Qt 6** 的开源 Word 文档编辑器，目标：

- 打开 / 编辑 / 保存 **`.docx`**（Office Open XML / WordprocessingML）。
- 自研 **OOXML 引擎**（零外部依赖的 ZIP 读写 + RFC1951 解压），完全掌控文档模型。
- 接入 **OpenAI 兼容大模型**（OpenAI / DeepSeek / Qwen / Ollama 等），实现 AI 自动编辑。

> 路线说明：第一版聚焦「常见 docx 能开能编能存 + 把 LLM 自动编辑做扎实」。
> 像素级 100% 保真需要长期迭代，当前覆盖日常 60–80% 文档（段落、字体、基础表格、图片占位待补）。

## 架构

```
src/
  core/        OOXML 文档模型 + 零依赖 ZIP/DEFLATE 工具（ziputil）
  io/          docx 读取（DocxReader）/ 写入（DocxWriter）
  ui/          编辑器控件（EditorWidget，模型↔QTextDocument）+ AI 面板（AIPanel）
  ai/          AIProvider 插件接口 + OpenAIProvider + AIManager（配置持久化）
```

数据流：`docx 文件 → ZipUtil 解包 → DocxReader 解析 XML → OOXML::Document 模型
→ EditorWidget 渲染/编辑 → toModel() 回写 → DocxWriter 生成合法 docx`。

## 构建

需要 **Qt 6.5+**（Core / Widgets / Network / Gui）与 **CMake 3.16+**，编译器 MSVC / GCC / Clang 均可。

### Windows（Qt 官方套件，MSVC）
```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.11.2\msvc2022_64"
cmake --build build --config Release
```

### Linux / macOS
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(qt-cmake 2>/dev/null; echo $Qt6_DIR)"
cmake --build build
```

构建会同时生成 `zip_test` 控制台工具，用于验证内置 DEFLATE 解压正确性。

## 使用

1. 启动程序，「文件 → 打开」选择一个 `.docx`。
2. 在编辑区直接修改文字、加粗/斜体/字号、插入表格。
3. 右侧 **AI 助手** 面板：
   - 填写 Base URL（默认 `https://api.openai.com/v1`）、API Key、模型名。
   - 选中一段文字 → 点「改写 / 润色 / 总结 / 翻译 / 扩写」→ 结果出现在下方 → 「替换选中」或「插入到光标」。
   - 「文档对话」：基于整篇文档向大模型提问。
4. 「文件 → 保存 / 另存为」导出 `.docx`。

## WPS 兼容

WPS 原生格式为私有格式，未公开。以 `.docx` 为兼容目标即可——WPS 可直接打开本项目保存的
`.docx`，也可把 WPS 文档「另存为 .docx」后用本项目打开。`.doc`（2003 二进制）暂未支持，
建议先用 Word/WPS 另存为 `.docx`。

## 路线图

- [x] 工程骨架 + 零依赖 ZIP/DEFLATE
- [x] OOXML 模型 + docx 读/写
- [x] 编辑器（段落/格式/表格）+ 打开保存
- [x] AI Provider（OpenAI 兼容）+ 选中操作 + 文档对话
- [ ] 图片 / 页眉页脚 / 分节
- [ ] 更好的表格与列表保真
- [ ] `.doc` 二进制读取
- [ ] LOK 升级接口（可选，冲全保真）

## 许可证

[MIT](LICENSE) — 任何人可自由使用、修改、闭源商用。
