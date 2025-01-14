import tkinter as tk
from tkinter import ttk

def main():
    root = tk.Tk()
    root.title("Tkinter Options")

    notebook = ttk.Notebook(root)
    notebook.pack(pady=10, expand=True)

    # Basic Widgets Frame
    basic_frame = ttk.Frame(notebook, width=400, height=280)
    basic_frame.pack(fill='both', expand=True)
    notebook.add(basic_frame, text='Basic Widgets')

    ttk.Label(basic_frame, text="Button").pack(pady=5)
    ttk.Button(basic_frame, text="Button").pack(pady=5)

    ttk.Label(basic_frame, text="Checkbox").pack(pady=5)
    ttk.Checkbutton(basic_frame, text="Checkbox").pack(pady=5)

    ttk.Label(basic_frame, text="RadioButton").pack(pady=5)
    ttk.Radiobutton(basic_frame, text="RadioButton", value=1).pack(pady=5)

    ttk.Label(basic_frame, text="Combo").pack(pady=5)
    ttk.Combobox(basic_frame, values=["Option 1", "Option 2", "Option 3"]).pack(pady=5)

    ttk.Label(basic_frame, text="InputText").pack(pady=5)
    ttk.Entry(basic_frame).pack(pady=5)

    ttk.Label(basic_frame, text="SliderFloat").pack(pady=5)
    ttk.Scale(basic_frame, from_=0.0, to=1.0, orient='horizontal').pack(pady=5)

    ttk.Label(basic_frame, text="ColorEdit3").pack(pady=5)
    ttk.Label(basic_frame, text="Color Picker not available in Tkinter").pack(pady=5)

    # Advanced Widgets Frame
    advanced_frame = ttk.Frame(notebook, width=400, height=280)
    advanced_frame.pack(fill='both', expand=True)
    notebook.add(advanced_frame, text='Advanced Widgets')

    ttk.Label(advanced_frame, text="PlotLines").pack(pady=5)
    ttk.Label(advanced_frame, text="PlotLines not available in Tkinter").pack(pady=5)

    ttk.Label(advanced_frame, text="PlotHistogram").pack(pady=5)
    ttk.Label(advanced_frame, text="PlotHistogram not available in Tkinter").pack(pady=5)

    ttk.Label(advanced_frame, text="TreeNode").pack(pady=5)
    tree = ttk.Treeview(advanced_frame)
    tree.insert("", "end", "node1", text="TreeNode Content")
    tree.pack(pady=5)

    root.mainloop()

if __name__ == "__main__":
    main()