import matplotlib.pyplot as plt
import matplotlib.patches as patches

# 1. 初始化画布，设置宽扁比例
fig, ax = plt.subplots(figsize=(14, 7))
ax.set_xlim(0, 14)
ax.set_ylim(0, 7)
ax.axis('off') # 关闭坐标轴显示

# ================= 辅助绘图函数 =================
# 绘制带圆角的彩色方块
def draw_block(x, y, w, h, text, bg='#DAE8FC', ec='#6C8EBF', fsize=10):
    rect = patches.FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.1", fc=bg, ec=ec, lw=1.5, zorder=3)
    ax.add_patch(rect)
    ax.text(x + w/2, y + h/2, text, ha='center', va='center', fontsize=fsize, fontweight='bold', family='sans-serif', zorder=4)

# 绘制圆形（用于减法器）
def draw_circle(x, y, r, text, bg='#F8CECC', ec='#B85450'):
    circle = patches.Circle((x, y), r, fc=bg, ec=ec, lw=1.5, zorder=3)
    ax.add_patch(circle)
    ax.text(x, y, text, ha='center', va='center', fontsize=14, fontweight='bold', zorder=4)

# 绘制带有灰色虚线边界的模块大框
def draw_module_box(x, y, w, h, title):
    rect = patches.Rectangle((x, y), w, h, fill=False, ec='gray', ls='--', lw=1.5, zorder=1)
    ax.add_patch(rect)
    ax.text(x + 0.1, y + h - 0.2, title, ha='left', va='top', fontsize=11, fontweight='bold', color='gray', style='italic', zorder=2)

# ================= 开始绘制 =================

# --- 绘制四大逻辑模块的虚线背景框 ---
draw_module_box(0.2, 3.8, 5.8, 2.5, "Stage 1: Preprocessing")
draw_module_box(6.2, 1.8, 4.4, 4.5, "Stage 2: AC/DC Separation (IIR Filtering)")
draw_module_box(10.8, 3.8, 3.0, 2.5, "Stage 3: Feature Extraction")
draw_module_box(6.2, 0.2, 7.6, 1.4, "Stage 4: Physiological Calculation")

# --- 绘制实体功能块 ---
# 阶段 1
draw_block(0.5, 4.5, 2.0, 0.8, "Raw ADC Data\n(Red & IR)", bg='#F5F5F5', ec='#666666')
draw_block(3.3, 4.5, 2.4, 0.8, "Moving Average Filter\n(High-freq Noise Removal)", bg='#DAE8FC', ec='#6C8EBF')

# 阶段 2 (交直流分离 - 论文核心)
draw_block(6.5, 2.2, 2.6, 0.8, "Ultra-Low Pass Filter\n(DC Baseline Extraction)\nα = 0.0124", bg='#D5E8D4', ec='#82B366')
draw_circle(7.8, 4.9, 0.3, "−") # 减法器
draw_block(8.8, 4.5, 1.6, 0.8, "Low Pass Filter\n(AC Smoothing)\nα = 0.2390", bg='#D5E8D4', ec='#82B366')

# 阶段 3 (寻找波峰波谷)
draw_block(11.0, 4.5, 2.6, 0.8, "Adaptive Thresholding &\nHysteresis State Machine", bg='#FFE6CC', ec='#D79B00')

# 阶段 4 (生理计算)
draw_block(11.0, 0.5, 2.6, 0.8, "Heart Rate (BPM)\n= 60 * fs / Interval", bg='#E1D5E7', ec='#9673A6')
draw_block(6.5, 0.5, 3.5, 0.8, "SpO2 Estimation\nR = (AC_red / DC_red) / (AC_ir / DC_ir)", bg='#E1D5E7', ec='#9673A6')

# --- 绘制逻辑连线 (Arrows) ---
style = "->"

# 数据流入 MA 滤波
ax.annotate("", xy=(3.3, 4.9), xytext=(2.5, 4.9), arrowprops=dict(arrowstyle=style, lw=1.5))

# MA 滤波分离出两条路：一条去减法器，一条去算DC
ax.annotate("", xy=(7.5, 4.9), xytext=(5.7, 4.9), arrowprops=dict(arrowstyle=style, lw=1.5))
ax.annotate("", xy=(6.5, 2.6), xytext=(4.5, 4.5), arrowprops=dict(arrowstyle=style, lw=1.5, connectionstyle="angle,angleA=180,angleB=-90,rad=10"))

# DC 提取出来后送入减法器 (做减法强行归零)
ax.annotate("", xy=(7.8, 4.6), xytext=(7.8, 3.0), arrowprops=dict(arrowstyle=style, lw=1.5))
ax.text(8.0, 3.8, "DC Component", rotation=90, ha='left', va='center', fontsize=9, fontweight='bold', color='#82B366')

# 减完后送去低通平滑 AC
ax.annotate("", xy=(8.8, 4.9), xytext=(8.1, 4.9), arrowprops=dict(arrowstyle=style, lw=1.5))

# 干净的 AC 送去状态机寻峰
ax.annotate("", xy=(11.0, 4.9), xytext=(10.4, 4.9), arrowprops=dict(arrowstyle=style, lw=1.5))
ax.text(10.7, 5.1, "AC Component", ha='center', va='bottom', fontsize=9, fontweight='bold', color='#82B366')

# 状态机算出峰间距 (Interval) 送去算心率
ax.annotate("", xy=(12.3, 1.3), xytext=(12.3, 4.5), arrowprops=dict(arrowstyle=style, lw=1.5))

# 状态机提供 AC 振幅送去算血氧
ax.annotate("", xy=(10.0, 0.9), xytext=(11.0, 4.5), arrowprops=dict(arrowstyle=style, lw=1.5, connectionstyle="angle,angleA=90,angleB=180,rad=10"))
ax.text(10.5, 2.5, "Peak/Valley\nTiming & AC Ampl", rotation=90, ha='right', va='center', fontsize=9, fontweight='bold')

# DC 也要送去算血氧！(用灰色虚线表示，体现算法严谨性)
ax.annotate("", xy=(8.25, 1.3), xytext=(7.8, 2.2), arrowprops=dict(arrowstyle=style, lw=1.5, ls='--', color='gray'))

# 保存并展示
plt.tight_layout()
plt.savefig('flowchart_3C.png', dpi=300, bbox_inches='tight')
print("图表已成功保存为 flowchart_3C.png")