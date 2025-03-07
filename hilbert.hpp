#ifndef HILBERT_H
#define HILBERT_H

struct hilbert_point {
    void *userdata;
    double x, y;
    double dist(const hilbert_point &p) const {
        return sqrt((x-p.x)*(x-p.x) + (y-p.y)*(y-p.y));
    }
    double fast_dist(const hilbert_point &p) const {
        double d = ((x-p.x)*(x-p.x) + (y-p.y)*(y-p.y));
        return d;
    }
};


// Where to curve to on an iteration
enum class hilbert_dir : int {
    cw_up=0, cw_left, cw_down, cw_right,
    ccw_up, ccw_left, ccw_down, ccw_right
};
const char *hilbert_dir_name(hilbert_dir d) {
    switch(d) {
    case hilbert_dir::cw_up: return "cw_up";
    case hilbert_dir::ccw_up: return "ccw_up";
    case hilbert_dir::cw_down: return "cw_down";
    case hilbert_dir::ccw_down: return "ccw_down";
    case hilbert_dir::cw_left: return "cw_left";
    case hilbert_dir::ccw_left: return "ccw_left";
    case hilbert_dir::cw_right: return "cw_right";
    case hilbert_dir::ccw_right: return "ccw_right";
    }
    return "???";
};

vector<hilbert_point> vcat(const vector<hilbert_point> &a, const vector<hilbert_point> &b, const vector<hilbert_point> &c, const vector<hilbert_point> &d) {
    vector<hilbert_point> r = a;
    r.insert(r.end(), b.begin(), b.end());
    r.insert(r.end(), c.begin(), c.end());
    r.insert(r.end(), d.begin(), d.end());
    return r;
}

vector<hilbert_point> hilbert_sub(vector<hilbert_point> vp, double left, double right, double top, double bottom, hilbert_dir dir) {
    if (false) cerr << "hilbert_sub("
        << "vp[" << vp.size() << "], "
        << left << ", "
        << right << ", "
        << top << ", "
        << bottom << ", "
        << hilbert_dir_name(dir) << ");" << endl << flush;

    // Sorting completed
    if (vp.size() <= 1)
        return vp;

    // Don't bother sorting hilbert_points too close to each-other,
    // and break infinite loops for coincident hilbert_points.
    if (right - left < 1 && top - bottom < 1)
        return vp;

    vector<hilbert_point> tl, tr, bl, br;

    double h_center = (right + left) / 2;
    double v_center = (top + bottom) / 2;

    for (auto p : vp) {
        if (p.x < h_center)
            if (p.y < v_center)
                bl.push_back(p);
            else
                tl.push_back(p);
        else
            if (p.y < v_center)
                br.push_back(p);
            else
                tr.push_back(p);
    }

    switch (dir) {
        case hilbert_dir::cw_up:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::cw_up);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::ccw_right);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::cw_up);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::ccw_left);

            vp = vcat(bl, tl, tr, br);
            break;

        case hilbert_dir::ccw_up:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::ccw_up);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::cw_right);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::ccw_up);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::cw_left);

            vp = vcat(br, tr, tl, bl);
            break;

        case hilbert_dir::cw_left:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::cw_left);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::cw_left);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::ccw_down);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::ccw_up);

            vp = vcat(br, bl, tl, tr);
            break;

        case hilbert_dir::ccw_left:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::ccw_left);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::ccw_left);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::cw_down);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::cw_up);

            vp = vcat(tr, tl, bl, br);
            break;

        case hilbert_dir::cw_right:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::ccw_down);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::ccw_up);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::cw_right);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::cw_right);

            vp = vcat(tl, tr, br, bl);
            break;

        case hilbert_dir::ccw_right:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::cw_down);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::cw_up);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::ccw_right);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::ccw_right);

            vp = vcat(bl, br, tr, tl);
            break;

        case hilbert_dir::cw_down:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::ccw_right);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::cw_down);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::ccw_left);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::cw_down);

            vp = vcat(tr, br, bl, tl);
            break;

        case hilbert_dir::ccw_down:
            tl = hilbert_sub(tl, left,     h_center, top,      v_center, hilbert_dir::cw_right);
            bl = hilbert_sub(bl, left,     h_center, v_center, bottom,   hilbert_dir::ccw_down);
            tr = hilbert_sub(tr, h_center, right,    top,      v_center, hilbert_dir::cw_left);
            br = hilbert_sub(br, h_center, right,    v_center, bottom,   hilbert_dir::ccw_down);

            vp = vcat(tl, bl, br, tr);
            break;

        default:
            cerr << "!!!";
            vp.clear();
    }

    return vp;
}

void hilbert_sort(vector<hilbert_point> &vp) {
    double top    = -INFINITY;
    double left   = +INFINITY;
    double right  = -INFINITY;
    double bottom = +INFINITY;
    for (auto &p : vp) {
        if (top    < p.y) top    = p.y;
        if (bottom > p.y) bottom = p.y;
        if (left   > p.x) left   = p.x;
        if (right  < p.x) right  = p.x;
    }

    vp = hilbert_sub(vp, left, right, top, bottom, hilbert_dir::cw_up);
}

#endif // HILBERT_H
