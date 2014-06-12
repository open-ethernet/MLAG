Creator	"yFiles"
Version	"2.11"
graph
[
	hierarchic	1
	label	""
	directed	1
	node
	[
		id	0
		label	"idle (d) "
		graphics
		[
			x	18.0
			y	-97.8505859375
			w	160.0
			h	80.0
			type	"roundrectangle"
			hasFill	0
			outline	"#000080"
			outlineWidth	2
		]
		LabelGraphics
		[
			text	"idle (d) "
			fontSize	12
			fontName	"Dialog"
			anchor	"c"
		]
	]
	node
	[
		id	1
		label	"mlag_enable  "
		graphics
		[
			x	737.5
			y	-97.8505859375
			w	981.0
			h	535.701171875
			type	"roundrectangle"
			hasFill	0
			outline	"#008000"
			outlineWidth	2
			topBorderInset	102.0
			bottomBorderInset	46.0
			leftBorderInset	108.0
			rightBorderInset	190.0
		]
		LabelGraphics
		[
			text	"mlag_enable  "
			fontSize	12
			fontName	"Dialog"
			anchor	"tr"
		]
		isGroup	1
	]
	node
	[
		id	2
		label	"master (d) "
		graphics
		[
			x	943.0
			y	69.0
			w	160.0
			h	80.0
			type	"roundrectangle"
			hasFill	0
			outline	"#000080"
			outlineWidth	2
		]
		LabelGraphics
		[
			text	"master (d) "
			fontSize	12
			fontName	"Dialog"
			anchor	"c"
		]
		gid	1
	]
	node
	[
		id	3
		label	"slave  "
		graphics
		[
			x	450.0
			y	69.0
			w	160.0
			h	80.0
			type	"roundrectangle"
			hasFill	0
			outline	"#008000"
			outlineWidth	2
		]
		LabelGraphics
		[
			text	"slave  "
			fontSize	12
			fontName	"Dialog"
			anchor	"c"
		]
		gid	1
	]
	node
	[
		id	4
		label	"standalone  "
		graphics
		[
			x	450.0
			y	-190.0
			w	160.0
			h	80.0
			type	"roundrectangle"
			hasFill	0
			outline	"#008000"
			outlineWidth	2
		]
		LabelGraphics
		[
			text	"standalone  "
			fontSize	12
			fontName	"Dialog"
			anchor	"c"
		]
		gid	1
	]
	node
	[
		id	5
		label	"is_master  "
		graphics
		[
			x	915.0
			y	-193.25
			w	174.0
			h	73.5
			type	"ellipse"
			fill	"#008000"
			outline	"#008000"
			outlineWidth	2
		]
		LabelGraphics
		[
			text	"is_master  "
			fontSize	12
			fontName	"Dialog"
			anchor	"t"
		]
		gid	1
	]
	edge
	[
		source	1
		target	0
		label	"tr50,ln147 : NULL "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
		]
		LabelGraphics
		[
			text	"tr50,ln147 : NULL "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	245.91459833648128
			y	-99.85058575871906
		]
		LabelGraphics
		[
			text	"stop_ev  "
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	147.08715820312523
			y	-119.11010742187551
		]
	]
	edge
	[
		source	1
		target	5
		label	"tr51,ln148 : NULL "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
			Line
			[
				point
				[
					x	737.5
					y	-97.8505859375
				]
				point
				[
					x	914.5
					y	-392.5
				]
				point
				[
					x	989.5
					y	-392.5
				]
				point
				[
					x	915.0
					y	-193.25
				]
			]
		]
		edgeAnchor
		[
			xSource	0.36085626911314983
			ySource	-1.0000364592258248
			xTarget	0.44116903134681446
			yTarget	-0.8976412771751372
		]
		LabelGraphics
		[
			text	"tr51,ln148 : NULL "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	912.5
			y	-367.7109375
		]
		LabelGraphics
		[
			text	"config_change_ev  "
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	920.1679146362474
			y	-325.25302958552504
		]
	]
	edge
	[
		source	0
		target	5
		label	"tr00,ln111 : NULL "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
			Line
			[
				point
				[
					x	18.0
					y	-97.8505859375
				]
				point
				[
					x	18.0
					y	-314.5
				]
				point
				[
					x	489.5
					y	-314.5
				]
				point
				[
					x	886.0
					y	-307.5
				]
				point
				[
					x	915.0
					y	-193.25
				]
			]
		]
		edgeAnchor
		[
			xTarget	-0.3333333333333333
			yTarget	-0.9436649659863954
		]
		LabelGraphics
		[
			text	"tr00,ln111 : NULL "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	16.000000000000004
			y	-139.82818603515625
		]
		LabelGraphics
		[
			text	"start_ev  "
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	119.81591796874994
			y	-337.110107421875
		]
	]
	edge
	[
		source	2
		target	2
		label	"tr10,ln117 :  fsm->peer_status = ev->state;  "
		graphics
		[
			type	"arc"
			fill	"#FF0000"
			targetArrow	"standard"
			arcType	"fixedRatio"
			arcHeight	32.09943771362305
			arcRatio	2.4572129249572754
			Line
			[
				point
				[
					x	943.0
					y	69.0
				]
				point
				[
					x	1052.7227783203125
					y	69.77623748779297
				]
				point
				[
					x	943.0
					y	69.0
				]
			]
		]
		edgeAnchor
		[
			xSource	0.94375
			ySource	-0.5625
			xTarget	0.9997915493886822
			yTarget	0.7390181305162045
		]
		LabelGraphics
		[
			text	"tr10,ln117 :  fsm->peer_status = ev->state;  "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	1021.0191040039062
			y	46.146724700927734
		]
		LabelGraphics
		[
			text	"peer_status_change_ev  "
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	1061.546630859375
			y	62.889892578125
		]
	]
	edge
	[
		source	2
		target	2
		label	" send_notification_msg:

    int err = 0;
	UNUSED_PARAM(ev);

	fsm_trace((struct fsm_base *)fsm, &quot;Enter to IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

	if ((fsm->my_ip_addr   != 0) &amp;&amp;
	    (fsm->peer_ip_addr != 0)) {

		if (fsm->peer_status == HEALTH_PEER_DOWN) {
			/* peer down */
			if (fsm->current_status == MASTER) {
				/* already Master */
				fsm->previous_status = MASTER;
			}
			else if (fsm->current_status == STANDALONE) {
				/* already STANDALONE */
				fsm->previous_status = STANDALONE;
			}
		}
		else {
			/* Can choose master */
			if (fsm->my_ip_addr < fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm	->current_status = SLAVE;
			}
			else if (fsm->my_ip_addr > fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm->current_status = MASTER;
			}
			else {
				/* the same ip addr */
				MLAG_LOG(MLAG_LOG_ERROR, &quot;Local ip address is the same with peer ip address on IPL L3 interface\n&quot;);
			}
		}
	}
	else {
		fsm->previous_status = fsm->current_status;
		fsm->current_status = STANDALONE;
	}

	fsm_trace((struct fsm_base *)fsm, &quot;Exit from IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

    return err;
 "
		graphics
		[
			type	"arc"
			width	2
			style	"dotted"
			fill	"#000000"
			arcType	"fixedRatio"
			arcHeight	-15.204710960388184
			arcRatio	-0.7599999904632568
			Line
			[
				point
				[
					x	943.0
					y	69.0
				]
				point
				[
					x	869.5233154296875
					y	69.0
				]
				point
				[
					x	943.0
					y	69.0
				]
			]
		]
		edgeAnchor
		[
			xSource	-0.7284
			ySource	-1.00031
			xTarget	-0.7284
			yTarget	1.00031
		]
		LabelGraphics
		[
			text	" send_notification_msg:

    int err = 0;
	UNUSED_PARAM(ev);

	fsm_trace((struct fsm_base *)fsm, &quot;Enter to IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

	if ((fsm->my_ip_addr   != 0) &amp;&amp;
	    (fsm->peer_ip_addr != 0)) {

		if (fsm->peer_status == HEALTH_PEER_DOWN) {
			/* peer down */
			if (fsm->current_status == MASTER) {
				/* already Master */
				fsm->previous_status = MASTER;
			}
			else if (fsm->current_status == STANDALONE) {
				/* already STANDALONE */
				fsm->previous_status = STANDALONE;
			}
		}
		else {
			/* Can choose master */
			if (fsm->my_ip_addr < fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm	->current_status = SLAVE;
			}
			else if (fsm->my_ip_addr > fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm->current_status = MASTER;
			}
			else {
				/* the same ip addr */
				MLAG_LOG(MLAG_LOG_ERROR, &quot;Local ip address is the same with peer ip address on IPL L3 interface\n&quot;);
			}
		}
	}
	else {
		fsm->previous_status = fsm->current_status;
		fsm->current_status = STANDALONE;
	}

	fsm_trace((struct fsm_base *)fsm, &quot;Exit from IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

    return err;
 "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	882.72802734375
			y	26.987600326538086
		]
		LabelGraphics
		[
			text	" "
			fontSize	6
			fontName	"Dialog"
			model	"side_slider"
			x	863.0
			y	29.0
		]
	]
	edge
	[
		source	3
		target	4
		label	"tr20,ln124 : on_master_down (NULL) 

    UNUSED_PARAM(parameter);
    SET_EVENT(mlag_master_election_fsm, peer_status_change_ev);
    UNUSED_PARAM(ev);
	fsm->current_status  = STANDALONE;
	fsm->previous_status = SLAVE;
	fsm->peer_status     = HEALTH_PEER_DOWN;
    return 0;
 "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
		]
		LabelGraphics
		[
			text	"tr20,ln124 : on_master_down (NULL) 

    UNUSED_PARAM(parameter);
    SET_EVENT(mlag_master_election_fsm, peer_status_change_ev);
    UNUSED_PARAM(ev);
	fsm->current_status  = STANDALONE;
	fsm->previous_status = SLAVE;
	fsm->peer_status     = HEALTH_PEER_DOWN;
    return 0;
 "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	448.0
			y	27.0054931640625
		]
		LabelGraphics
		[
			text	"peer_status_change_ev 
[ ev->state == HEALTH_PEER_DOWN ]"
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	356.54052734375
			y	-77.72021484375
		]
	]
	edge
	[
		source	3
		target	3
		label	" send_notification_msg:

    int err = 0;
	UNUSED_PARAM(ev);

	send_switch_status_change_event(fsm);
    return err;
 "
		graphics
		[
			type	"arc"
			width	2
			style	"dotted"
			fill	"#000000"
			arcType	"fixedRatio"
			arcHeight	-15.204710960388184
			arcRatio	-0.7599999904632568
			Line
			[
				point
				[
					x	450.0
					y	69.0
				]
				point
				[
					x	376.5232849121094
					y	69.0
				]
				point
				[
					x	450.0
					y	69.0
				]
			]
		]
		edgeAnchor
		[
			xSource	-0.7284
			ySource	-1.00031
			xTarget	-0.7284
			yTarget	1.00031
		]
		LabelGraphics
		[
			text	" send_notification_msg:

    int err = 0;
	UNUSED_PARAM(ev);

	send_switch_status_change_event(fsm);
    return err;
 "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	389.7279968261719
			y	26.987600326538086
		]
		LabelGraphics
		[
			text	" "
			fontSize	6
			fontName	"Dialog"
			model	"side_slider"
			x	370.0
			y	29.0
		]
	]
	edge
	[
		source	4
		target	5
		label	"tr30,ln131 :  fsm->peer_status = ev->state;  "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
			Line
			[
				point
				[
					x	450.0
					y	-190.0
				]
				point
				[
					x	450.0
					y	-277.5
				]
				point
				[
					x	663.5
					y	-277.5
				]
				point
				[
					x	915.0
					y	-193.25
				]
			]
		]
		LabelGraphics
		[
			text	"tr30,ln131 :  fsm->peer_status = ev->state;  "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	448.1179915991855
			y	-272.00888617076464
		]
		LabelGraphics
		[
			text	"peer_status_change_ev  "
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	488.546630859375
			y	-294.110107421875
		]
	]
	edge
	[
		source	4
		target	4
		label	" send_notification_msg:

    int err = 0;
	UNUSED_PARAM(ev);

	send_switch_status_change_event(fsm);
    return err;
 "
		graphics
		[
			type	"arc"
			width	2
			style	"dotted"
			fill	"#000000"
			arcType	"fixedRatio"
			arcHeight	-15.204713821411133
			arcRatio	-0.7599999904632568
			Line
			[
				point
				[
					x	450.0
					y	-190.0
				]
				point
				[
					x	376.5232849121094
					y	-190.0
				]
				point
				[
					x	450.0
					y	-190.0
				]
			]
		]
		edgeAnchor
		[
			xSource	-0.7284
			ySource	-1.00031
			xTarget	-0.7284
			yTarget	1.00031
		]
		LabelGraphics
		[
			text	" send_notification_msg:

    int err = 0;
	UNUSED_PARAM(ev);

	send_switch_status_change_event(fsm);
    return err;
 "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	389.7279968261719
			y	-232.0124053955078
		]
		LabelGraphics
		[
			text	" "
			fontSize	6
			fontName	"Dialog"
			model	"side_slider"
			x	370.0
			y	-230.0
		]
	]
	edge
	[
		source	5
		target	2
		label	"tr40,ln138 : NULL "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
		]
		LabelGraphics
		[
			text	"tr40,ln138 : NULL "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	916.9197387695312
			y	-158.53732299804688
		]
		LabelGraphics
		[
			text	"  
[ is_master(fsm) ]"
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	888.282958984375
			y	-107.72021484375
		]
	]
	edge
	[
		source	5
		target	3
		label	"tr41,ln139 : NULL "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
		]
		LabelGraphics
		[
			text	"tr41,ln139 : NULL "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	860.8450317382812
			y	-165.8357391357422
		]
		LabelGraphics
		[
			text	"  
[ is_slave(fsm)  ]"
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	733.32666015625
			y	-129.72021484375
		]
	]
	edge
	[
		source	5
		target	4
		label	"tr42,ln140 : NULL "
		graphics
		[
			fill	"#FF0000"
			targetArrow	"standard"
		]
		LabelGraphics
		[
			text	"tr42,ln140 : NULL "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	826.0216174919657
			y	-193.81478482021086
		]
		LabelGraphics
		[
			text	"  
[ else           ]"
			fontSize	11
			fontName	"Miriam"
			model	"free"
			x	656.726318359375
			y	-219.72021484374997
		]
	]
	edge
	[
		source	5
		target	5
		label	" calc_switch_status:

    int err = 0;
	UNUSED_PARAM(ev);

	fsm_trace((struct fsm_base *)fsm, &quot;Enter to IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

	if ((fsm->my_ip_addr   != 0) &amp;&amp;
	    (fsm->peer_ip_addr != 0)) {

		if (fsm->peer_status == HEALTH_PEER_DOWN) {
			/* peer down */
			if (fsm->current_status == MASTER) {
				/* already Master */
				fsm->previous_status = MASTER;
			}
			else if (fsm->current_status == STANDALONE) {
				/* already STANDALONE */
				fsm->previous_status = STANDALONE;
			}
		}
		else {
			/* Can choose master */
			if (fsm->my_ip_addr < fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm	->current_status = SLAVE;
			}
			else if (fsm->my_ip_addr > fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm->current_status = MASTER;
			}
			else {
				/* the same ip addr */
				MLAG_LOG(MLAG_LOG_ERROR, &quot;Local ip address is the same with peer ip address on IPL L3 interface\n&quot;);
			}
		}
	}
	else {
		fsm->previous_status = fsm->current_status;
		fsm->current_status = STANDALONE;
	}

	fsm_trace((struct fsm_base *)fsm, &quot;Exit from IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

    return err;
 "
		graphics
		[
			type	"arc"
			width	2
			style	"dotted"
			fill	"#000000"
			arcType	"fixedRatio"
			arcHeight	-13.969330787658691
			arcRatio	-0.7599999904632568
			Line
			[
				point
				[
					x	915.0
					y	-193.25
				]
				point
				[
					x	837.6598510742188
					y	-193.25
				]
				point
				[
					x	915.0
					y	-193.25
				]
			]
		]
		edgeAnchor
		[
			xSource	-0.7284
			ySource	-1.00031
			xTarget	-0.7284
			yTarget	1.00031
		]
		LabelGraphics
		[
			text	" calc_switch_status:

    int err = 0;
	UNUSED_PARAM(ev);

	fsm_trace((struct fsm_base *)fsm, &quot;Enter to IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

	if ((fsm->my_ip_addr   != 0) &amp;&amp;
	    (fsm->peer_ip_addr != 0)) {

		if (fsm->peer_status == HEALTH_PEER_DOWN) {
			/* peer down */
			if (fsm->current_status == MASTER) {
				/* already Master */
				fsm->previous_status = MASTER;
			}
			else if (fsm->current_status == STANDALONE) {
				/* already STANDALONE */
				fsm->previous_status = STANDALONE;
			}
		}
		else {
			/* Can choose master */
			if (fsm->my_ip_addr < fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm	->current_status = SLAVE;
			}
			else if (fsm->my_ip_addr > fsm->peer_ip_addr) {
				fsm->previous_status = fsm->current_status;
				fsm->current_status = MASTER;
			}
			else {
				/* the same ip addr */
				MLAG_LOG(MLAG_LOG_ERROR, &quot;Local ip address is the same with peer ip address on IPL L3 interface\n&quot;);
			}
		}
	}
	else {
		fsm->previous_status = fsm->current_status;
		fsm->current_status = STANDALONE;
	}

	fsm_trace((struct fsm_base *)fsm, &quot;Exit from IsMaster_entry_func, curr=%d, prev=%d\n&quot;, fsm->current_status, fsm->previous_status);

    return err;
 "
			fontSize	0
			fontName	"Dialog"
			model	"free"
			x	849.6292114257812
			y	-232.0113983154297
		]
		LabelGraphics
		[
			text	" "
			fontSize	6
			fontName	"Dialog"
			model	"side_slider"
			x	828.0
			y	-230.0
		]
	]
]
