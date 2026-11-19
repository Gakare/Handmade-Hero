vim.opt.makeprg = ".\\code\\build.bat"
vim.keymap.set("n", "<a-e>", "<cmd>copen<cr>")
vim.keymap.set("n", "<c-c>", "<cmd>make<cr>")
vim.opt.colorcolumn = "104"

-- TODO: This might be moved into my config
vim.keymap.set("x", "p", function()
	return 'pgv"' .. vim.v.register .. "y"
end, { remap = false, expr = true })

vim.opt.cindent = true
vim.opt.cinoptions = "{0,1s,:0,n-2,(s,u0,>s,g0,h0,-2s,=0,ps,ts,ws,-1s,.5s,n2,(0,^0,:s,+s"
