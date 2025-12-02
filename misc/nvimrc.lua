vim.keymap.set("n", "<c-c>", function()
	vim.cmd.TermSend(".\\code\\build.bat")
end)

vim.keymap.set("n", "<c-l>", function()
	vim.cmd.TermSend(".\\misc\\shell.bat")
end)
