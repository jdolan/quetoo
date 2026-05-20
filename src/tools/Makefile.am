EXTRA_DIST = \
	README.md \
	__init__.py \
	tools-env.sh \
	matfu.py \
	unpak.py \
	mdl2obj.py \
	md22obj.py \
	md32obj.py \
	objfu.py \
 	pyproject.toml \
	skyfu.py

.PHONY: install-tools
install-tools:
	python3 -m venv .venv
	.venv/bin/python3 -m pip install numpy pillow opencv-python scikit-image scipy
	.venv/bin/python3 -m pip install -e .
	@echo "Tools installed. Activate with: source src/tools/tools-env.sh"
