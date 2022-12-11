# mypstree

please keep academic integrity

just for <https://jyywiki.cn/OS/2022/labs/M1> homework

## 目前输出

```
init(1)-+-init(24)---init(25)-+-sh(26)---sh(27)---sh(32)---node(36)-+-node(47)-+-bash(1859)
                                                                               |-bash(1951)---my_pstree(8888)
                                                                               `-bash(2395)
                                                                    |-node(103)
                                                                    |-node(123)-+-node(514)
                                                                                `-cpptools(711)
                                                                    |-node(136)---cpptools(465)
                                                                    `-node(143)
                              |-cpptools-srv(783)
                              |-cpptools-srv(8779)
                              |-cpptools-srv(8867)
                              `-cpptools-srv(8887)
        |-init(76)---init(77)---node(78)
        |-init(85)---init(86)---node(87)
        |-init(94)---init(95)---node(96)
        `-init(114)---init(115)---node(116)
```

需要把枝干连上，也就是在每次输出兄弟节点（新的一行）的时候，我们需要去拿到父节点的所有分支节点（有多个子节点的）也就是+的偏移。
