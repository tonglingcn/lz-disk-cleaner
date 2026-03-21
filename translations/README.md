# 国际化翻译文件

## 支持的语言

- 简体中文 (zh_CN) - 默认
- 英文 (en_US)

## 生成翻译文件

```bash
# 1. 提取所有可翻译字符串
lupdate src -ts translations/lz-disk-cleaner_zh_CN.ts
lupdate src -ts translations/lz-disk-cleaner_en_US.ts

# 2. 使用 Qt Linguist 编辑翻译
linguist translations/lz-disk-cleaner_zh_CN.ts

# 3. 编译翻译文件
lrelease translations/lz-disk-cleaner_zh_CN.ts
lrelease translations/lz-disk-cleaner_en_US.ts
```

## 在代码中使用

所有用户可见的字符串都应该使用 `tr()` 包裹：

```cpp
// ✅ 正确
QString message = tr("清理完成");

// ❌ 错误
QString message = "清理完成";
```

## 翻译文件位置

- 源文件: `translations/*.ts`
- 编译后: `translations/*.qm`
- 资源文件: 在 `resources/resources.qrc` 中引用
